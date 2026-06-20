#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "ShiftXRTypes.h"
#include "ShiftXRHandInteractor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShiftGestureSignature, EShiftGesture, Gesture);

/**
 * Reads hand tracking (via the IHandTracker modular feature), derives an aim ray and
 * pinch/grab/push/shift gestures, and replicates a coarse pose for co-op. Attach two of
 * these to the pawn (one per hand). All gameplay-affecting gestures should be confirmed
 * server-side; this component exposes the local read + a throttled replicated snapshot.
 */
UCLASS(ClassGroup = (ShiftXR), meta = (BlueprintSpawnableComponent))
class SHIFTXRCORE_API UShiftXRHandInteractor : public UActorComponent
{
	GENERATED_BODY()

public:
	UShiftXRHandInteractor();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Which physical hand this interactor reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShiftXR")
	EControllerHand Hand = EControllerHand::Left;

	/** Thumb-tip to index-tip distance (cm) below which a pinch is registered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShiftXR", meta = (ClampMin = "0.5"))
	float PinchThresholdCm = 3.0f;

	/** Seconds between replicated pose updates (throttle; ~20 Hz default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShiftXR", meta = (ClampMin = "0.0"))
	float NetUpdateInterval = 0.05f;

	/** Palm speed (cm/s) above which a melee "swing" (Push) gesture is registered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShiftXR", meta = (ClampMin = "10.0"))
	float SwingSpeedThreshold = 150.f;

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	bool GetPalmTransform(FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	bool GetAimRay(FVector& OutOrigin, FVector& OutDirection) const;

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	float GetPinchStrength() const { return PinchStrength; }

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	float GetPalmSpeed() const { return PalmSpeed; }

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	EShiftGesture GetCurrentGesture() const { return CurrentGesture; }

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	bool IsHandTracked() const { return bTracked; }

	/** Fired when a new gesture begins. */
	UPROPERTY(BlueprintAssignable, Category = "ShiftXR")
	FShiftGestureSignature OnGestureStarted;

	/** Fired when the current gesture ends. */
	UPROPERTY(BlueprintAssignable, Category = "ShiftXR")
	FShiftGestureSignature OnGestureEnded;

protected:
	/** Throttled, replicated coarse pose used by remote clients. */
	UPROPERTY(ReplicatedUsing = OnRep_HandState)
	FShiftHandPose RepHandState;

	UFUNCTION()
	void OnRep_HandState();

	/** Client -> server push of the locally-tracked pose (throttled, unreliable). */
	UFUNCTION(Server, Unreliable)
	void ServerUpdateHandState(const FShiftHandPose& NewPose);

private:
	EShiftGesture CurrentGesture = EShiftGesture::None;
	bool bTracked = false;
	FTransform PalmTransform = FTransform::Identity;
	FVector AimOrigin = FVector::ZeroVector;
	FVector AimDirection = FVector::ForwardVector;
	float PinchStrength = 0.f;
	float PalmSpeed = 0.f;
	float TimeSinceNetUpdate = 0.f;

	bool IsLocallyControlledPawn() const;
	void UpdateFromHandTracking();
	EShiftGesture EvaluateGesture() const;
	void SetGesture(EShiftGesture NewGesture);
	void ApplyReplicatedPose();

	FVector PrevPalmLocation = FVector::ZeroVector;
	bool bHasPrevPalm = false;
};
