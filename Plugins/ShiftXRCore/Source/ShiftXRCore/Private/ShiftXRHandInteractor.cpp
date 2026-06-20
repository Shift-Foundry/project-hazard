#include "ShiftXRHandInteractor.h"
#include "ShiftXRCore.h"
#include "IHandTracker.h"
#include "HeadMountedDisplayTypes.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace
{
	IHandTracker* GetHandTrackerImpl()
	{
		IModularFeatures& MF = IModularFeatures::Get();
		const FName Feature = IHandTracker::GetModularFeatureName();
		if (MF.IsModularFeatureAvailable(Feature))
		{
			TArray<IHandTracker*> Impls = MF.GetModularFeatureImplementations<IHandTracker>(Feature);
			if (Impls.Num() > 0)
			{
				return Impls[0];
			}
		}
		return nullptr;
	}
}

UShiftXRHandInteractor::UShiftXRHandInteractor()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UShiftXRHandInteractor::BeginPlay()
{
	Super::BeginPlay();
}

void UShiftXRHandInteractor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UShiftXRHandInteractor, RepHandState);
}

bool UShiftXRHandInteractor::IsLocallyControlledPawn() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn->IsLocallyControlled();
	}
	// Non-pawn owners: treat the authority as the local driver.
	return GetOwner() && GetOwner()->HasAuthority();
}

void UShiftXRHandInteractor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsLocallyControlledPawn())
	{
		return; // remote proxies are driven by OnRep_HandState
	}

	UpdateFromHandTracking();

	// Palm speed -> swing (melee) gesture detection.
	if (bTracked)
	{
		if (bHasPrevPalm && DeltaTime > 0.f)
		{
			PalmSpeed = FVector::Dist(PalmTransform.GetLocation(), PrevPalmLocation) / DeltaTime;
		}
		PrevPalmLocation = PalmTransform.GetLocation();
		bHasPrevPalm = true;
	}
	else
	{
		PalmSpeed = 0.f;
		bHasPrevPalm = false;
	}
	SetGesture(EvaluateGesture());

	TimeSinceNetUpdate += DeltaTime;
	if (TimeSinceNetUpdate >= NetUpdateInterval)
	{
		TimeSinceNetUpdate = 0.f;

		FShiftHandPose Pose;
		Pose.PalmTransform = PalmTransform;
		Pose.Gesture = CurrentGesture;
		Pose.GripAlpha = PinchStrength;
		Pose.bTracked = bTracked;

		if (GetOwner() && GetOwner()->HasAuthority())
		{
			RepHandState = Pose;
		}
		else
		{
			ServerUpdateHandState(Pose);
		}
	}
}

void UShiftXRHandInteractor::UpdateFromHandTracking()
{
	IHandTracker* Tracker = GetHandTrackerImpl();
	if (!Tracker || !Tracker->IsHandTrackingStateValid())
	{
		bTracked = false;
		PinchStrength = 0.f;
		return;
	}

	// IHandTracker keypoints are returned in UE world space (see OpenXRHandTracking::GetKeypointState).
	FTransform PalmXf, WristXf, ThumbTipXf, IndexTipXf, IndexProxXf;
	float Radius = 0.f;

	const bool bPalm = Tracker->GetKeypointState(Hand, EHandKeypoint::Palm, PalmXf, Radius);
	Tracker->GetKeypointState(Hand, EHandKeypoint::Wrist, WristXf, Radius);
	const bool bThumb = Tracker->GetKeypointState(Hand, EHandKeypoint::ThumbTip, ThumbTipXf, Radius);
	const bool bIndex = Tracker->GetKeypointState(Hand, EHandKeypoint::IndexTip, IndexTipXf, Radius);
	const bool bIndexProx = Tracker->GetKeypointState(Hand, EHandKeypoint::IndexProximal, IndexProxXf, Radius);

	if (!bPalm)
	{
		bTracked = false;
		PinchStrength = 0.f;
		return;
	}

	bTracked = true;
	PalmTransform = PalmXf;

	// Aim ray: origin at palm, direction along wrist -> index-proximal (a stable "point" axis),
	// falling back to the palm forward vector.
	AimOrigin = PalmXf.GetLocation();
	FVector Dir = bIndexProx ? (IndexProxXf.GetLocation() - WristXf.GetLocation()) : FVector::ZeroVector;
	AimDirection = Dir.GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = PalmXf.GetRotation().GetForwardVector();
	}

	// Pinch strength from thumb-tip / index-tip separation.
	if (bThumb && bIndex)
	{
		const float DistCm = FVector::Dist(ThumbTipXf.GetLocation(), IndexTipXf.GetLocation());
		const float Range = FMath::Max(PinchThresholdCm * 2.f, 0.01f);
		PinchStrength = FMath::Clamp(1.f - (DistCm / Range), 0.f, 1.f);
	}
	else
	{
		PinchStrength = 0.f;
	}
}

EShiftGesture UShiftXRHandInteractor::EvaluateGesture() const
{
	if (!bTracked)
	{
		return EShiftGesture::None;
	}
	// A fast palm motion is a melee swing; otherwise a pinch is a ranged trigger.
	if (PalmSpeed > SwingSpeedThreshold)
	{
		return EShiftGesture::Push;
	}
	if (PinchStrength > 0.7f)
	{
		return EShiftGesture::Pinch;
	}
	return EShiftGesture::None;
}

void UShiftXRHandInteractor::SetGesture(EShiftGesture NewGesture)
{
	if (NewGesture == CurrentGesture)
	{
		return;
	}

	if (CurrentGesture != EShiftGesture::None)
	{
		OnGestureEnded.Broadcast(CurrentGesture);
	}

	CurrentGesture = NewGesture;

	if (CurrentGesture != EShiftGesture::None)
	{
		OnGestureStarted.Broadcast(CurrentGesture);
	}
}

void UShiftXRHandInteractor::ApplyReplicatedPose()
{
	PalmTransform = RepHandState.PalmTransform;
	PinchStrength = RepHandState.GripAlpha;
	bTracked = RepHandState.bTracked;
	SetGesture(RepHandState.Gesture);
}

void UShiftXRHandInteractor::OnRep_HandState()
{
	if (!IsLocallyControlledPawn())
	{
		ApplyReplicatedPose();
	}
}

void UShiftXRHandInteractor::ServerUpdateHandState_Implementation(const FShiftHandPose& NewPose)
{
	// Cosmetic pose from a client — sanity-check before storing (never trust raw client data,
	// and no gameplay/damage is derived from this replicated gesture).
	if (NewPose.PalmTransform.ContainsNaN())
	{
		return;
	}
	FShiftHandPose Clean = NewPose;
	Clean.GripAlpha = FMath::Clamp(Clean.GripAlpha, 0.f, 1.f);
	RepHandState = Clean;
}

bool UShiftXRHandInteractor::GetPalmTransform(FTransform& OutTransform) const
{
	OutTransform = PalmTransform;
	return bTracked;
}

bool UShiftXRHandInteractor::GetAimRay(FVector& OutOrigin, FVector& OutDirection) const
{
	OutOrigin = AimOrigin;
	OutDirection = AimDirection;
	return bTracked;
}
