#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ShiftXRTypes.h"
#include "ShiftXRSceneAnchor.generated.h"

/**
 * Backend-agnostic interface for scene understanding & anchors. Concrete backends:
 *  - FShiftSceneBackend_Stub      : works on stock UE 5.8 (transform anchors + physics-trace surface).
 *  - FShiftSceneBackend_AndroidXR : real planes/anchors/persistence (gated behind WITH_ANDROIDXR).
 *  - FShiftSceneBackend_OpenXRFB  : Meta Quest spatial-entity path (baseline compatibility).
 * Selected at component init; gameplay code only ever talks to UShiftXRSceneAnchor.
 */
class IShiftSceneBackend
{
public:
	virtual ~IShiftSceneBackend() {}
	virtual bool TryPlaceAnchor(const FTransform& WorldTransform, FShiftAnchorHandle& OutHandle) = 0;
	virtual bool GetAnchorTransform(const FShiftAnchorHandle& Handle, FTransform& OutTransform) const = 0;
	virtual bool PersistAnchor(const FShiftAnchorHandle& Handle) = 0;
	virtual bool RaycastEnvironment(const UWorld* World, const FVector& Origin, const FVector& Direction, float Length, FShiftSceneHit& OutHit) const = 0;
	virtual bool ExportAnchorForSharing(const FShiftAnchorHandle& Handle, TArray<uint8>& OutBlob) const = 0;
	virtual bool ImportSharedAnchor(const TArray<uint8>& Blob, FShiftAnchorHandle& OutHandle) = 0;
	virtual const TCHAR* GetBackendName() const = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShiftScenePlanesUpdated);

/**
 * Anchors an actor to a real-world surface (or a stand-in surface in the stub backend) and
 * exposes environment raycasts and co-op anchor sharing. On stock UE 5.8 this runs the stub
 * backend; the real Android XR backend swaps in when a 5.8-compatible plugin is available.
 */
UCLASS(ClassGroup = (ShiftXR), meta = (BlueprintSpawnableComponent))
class SHIFTXRCORE_API UShiftXRSceneAnchor : public USceneComponent
{
	GENERATED_BODY()

public:
	UShiftXRSceneAnchor();

	virtual void BeginPlay() override;

	/** Place an anchor at a world transform. Returns a backend handle. */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool TryPlaceAnchor(const FTransform& WorldTransform, FShiftAnchorHandle& OutHandle);

	/** Resolve the current world transform of a previously-placed anchor. */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool GetAnchorTransform(const FShiftAnchorHandle& Handle, FTransform& OutTransform) const;

	/** Request the backend persist an anchor across sessions (no-op in the stub). */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool PersistAnchor(const FShiftAnchorHandle& Handle);

	/** Drop this anchor onto the nearest surface below (stub: traces against world collision). */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool AnchorToSurfaceBelow(float MaxTraceDistance, FShiftAnchorHandle& OutHandle);

	/** Raycast against scene geometry (stub: physics line trace). */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool RaycastEnvironment(const FVector& Origin, const FVector& Direction, float Length, FShiftSceneHit& OutHit) const;

	/** Serialize an anchor for co-op colocation sharing (stub: encodes the transform). */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool ExportAnchorForSharing(const FShiftAnchorHandle& Handle, TArray<uint8>& OutBlob) const;

	/** Reconstruct a shared anchor on another client (stub: decodes the transform). */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	bool ImportSharedAnchor(const TArray<uint8>& Blob, FShiftAnchorHandle& OutHandle);

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	FString GetBackendName() const;

	/** Fired when scene planes/mesh update (stub never fires; real backends do). */
	UPROPERTY(BlueprintAssignable, Category = "ShiftXR")
	FShiftScenePlanesUpdated OnPlanesUpdated;

private:
	TUniquePtr<IShiftSceneBackend> Backend;
};
