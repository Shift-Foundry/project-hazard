#pragma once

#include "CoreMinimal.h"
#include "ShiftXRTypes.generated.h"

/** High-level hand gestures recognized by UShiftXRHandInteractor. */
UENUM(BlueprintType)
enum class EShiftGesture : uint8
{
	None		UMETA(DisplayName = "None"),
	Pinch		UMETA(DisplayName = "Pinch"),
	Grab		UMETA(DisplayName = "Grab"),
	Push		UMETA(DisplayName = "Push"),
	Shift		UMETA(DisplayName = "Shift")
};

/** Performance tiers driven by UShiftXRPerfGovernor. Ultra = best quality, Emergency = survival. */
UENUM(BlueprintType)
enum class EShiftPerfTier : uint8
{
	Ultra		UMETA(DisplayName = "Ultra"),
	High		UMETA(DisplayName = "High"),
	Medium		UMETA(DisplayName = "Medium"),
	Low			UMETA(DisplayName = "Low"),
	Emergency	UMETA(DisplayName = "Emergency")
};

/** Coarse, replication-friendly snapshot of a hand. The full 26-joint skeleton stays local. */
USTRUCT(BlueprintType)
struct FShiftHandPose
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	FTransform PalmTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	EShiftGesture Gesture = EShiftGesture::None;

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	float GripAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	bool bTracked = false;
};

/** Opaque handle to a placed scene anchor (backend-managed). */
USTRUCT(BlueprintType)
struct FShiftAnchorHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	int32 Id = INDEX_NONE;

	bool IsValid() const { return Id != INDEX_NONE; }
};

/** Result of an environment raycast against scene geometry. */
USTRUCT(BlueprintType)
struct FShiftSceneHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ShiftXR")
	FVector Normal = FVector::UpVector;
};
