#include "WeaponComponent.h"
#include "WeaponBase.h"
#include "ShiftXRHandInteractor.h"
#include "MotionControllerComponent.h"
#include "ZombieBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHazardAutoFire(
	TEXT("hazard.AutoFire"), 0,
	TEXT("Debug: weapon components auto-fire at the nearest zombie (1=on)."));

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWeaponComponent, Weapon);
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FActorSpawnParameters Params;
			Params.Owner = OwnerActor;
			Params.Instigator = Cast<APawn>(OwnerActor);
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			TSubclassOf<AWeaponBase> Cls = WeaponClass;
			if (!Cls)
			{
				Cls = AWeaponBase::StaticClass();
			}
			Weapon = World->SpawnActor<AWeaponBase>(Cls, FTransform::Identity, Params);
			if (Weapon)
			{
				if (!WeaponClass)
				{
					Weapon->Mode = FallbackMode; // C++ fallback when no BP weapon assigned
				}
				USceneComponent* Attach = AttachTarget ? AttachTarget.Get() : OwnerActor->GetRootComponent();
				if (Attach)
				{
					Weapon->AttachToComponent(Attach, FAttachmentTransformRules::SnapToTargetIncludingScale);
				}
			}
		}
	}

	if (HandRef)
	{
		HandRef->OnGestureStarted.AddDynamic(this, &UWeaponComponent::HandleGestureStarted);
	}
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The weapon is a separately-spawned actor (not a subobject); destroy it with the pawn
	// so it doesn't leak when a player leaves/respawns in co-op.
	if (Weapon && GetOwner() && GetOwner()->HasAuthority())
	{
		Weapon->Destroy();
	}
	Weapon = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarHazardAutoFire.GetValueOnGameThread() > 0)
	{
		AutoFireTick(DeltaTime);
	}
}

void UWeaponComponent::HandleGestureStarted(EShiftGesture Gesture)
{
	if (!Weapon)
	{
		return;
	}
	// Pinch fires ranged weapons; a swing (Push) triggers melee.
	const bool bRangedTrigger = Weapon->IsRanged() && Gesture == EShiftGesture::Pinch;
	const bool bMeleeTrigger = !Weapon->IsRanged() && Gesture == EShiftGesture::Push;
	if (bRangedTrigger || bMeleeTrigger)
	{
		TriggerPrimary();
	}
}

bool UWeaponComponent::GetAim(FVector& OutOrigin, FVector& OutDir) const
{
	// 1) OpenXR controller aim pose (6DoF controller, e.g. XREAL Aura) when tracked.
	if (AimController && AimController->IsTracked())
	{
		OutOrigin = AimController->GetComponentLocation();
		OutDir = AimController->GetForwardVector();
		return true;
	}
	// 2) Hand-tracking aim ray (hands-first device, e.g. Galaxy XR).
	if (HandRef && HandRef->IsHandTracked() && HandRef->GetAimRay(OutOrigin, OutDir))
	{
		return true;
	}
	// Desktop / no-tracking fallback: aim from the camera.
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const UCameraComponent* Cam = OwnerActor->FindComponentByClass<UCameraComponent>())
		{
			OutOrigin = Cam->GetComponentLocation();
			OutDir = Cam->GetForwardVector();
			return true;
		}
	}
	return false;
}

void UWeaponComponent::EquipWeaponDef(const FWeaponDef& Def)
{
	if (Weapon && GetOwner() && GetOwner()->HasAuthority())
	{
		Weapon->InitFromDef(Def);
	}
}

void UWeaponComponent::TriggerPrimary()
{
	if (!Weapon)
	{
		return;
	}
	FVector Origin, Dir;
	if (GetAim(Origin, Dir))
	{
		Weapon->TryFire(Origin, Dir, GetOwner());
	}
}

void UWeaponComponent::AutoFireTick(float DeltaTime)
{
	AActor* OwnerActor = GetOwner();
	if (!Weapon || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	// Aim from the camera (or owner) toward the nearest live zombie.
	FVector Origin = OwnerActor->GetActorLocation();
	if (const UCameraComponent* Cam = OwnerActor->FindComponentByClass<UCameraComponent>())
	{
		Origin = Cam->GetComponentLocation();
	}

	AZombieBase* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AZombieBase> It(GetWorld()); It; ++It)
	{
		AZombieBase* Z = *It;
		if (!IsValid(Z))
		{
			continue;
		}
		const float DSq = FVector::DistSquared(Origin, Z->GetActorLocation());
		if (DSq < BestDistSq)
		{
			BestDistSq = DSq;
			Nearest = Z;
		}
	}

	if (Nearest)
	{
		const FVector Dir = (Nearest->GetActorLocation() - Origin).GetSafeNormal();
		Weapon->TryFire(Origin, Dir, OwnerActor); // weapon gates by its own cooldown/ammo
	}
}
