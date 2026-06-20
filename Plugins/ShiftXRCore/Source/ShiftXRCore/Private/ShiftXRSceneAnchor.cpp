#include "ShiftXRSceneAnchor.h"
#include "ShiftXRCore.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace
{
	/**
	 * Stub backend that works on stock UE 5.8 (no scene-understanding runtime).
	 * Anchors are world transforms kept in a map; raycasts fall back to world collision.
	 */
	class FShiftSceneBackend_Stub : public IShiftSceneBackend
	{
	public:
		virtual bool TryPlaceAnchor(const FTransform& WorldTransform, FShiftAnchorHandle& OutHandle) override
		{
			const int32 NewId = NextId++;
			Anchors.Add(NewId, WorldTransform);
			OutHandle.Id = NewId;
			return true;
		}

		virtual bool GetAnchorTransform(const FShiftAnchorHandle& Handle, FTransform& OutTransform) const override
		{
			if (const FTransform* Found = Anchors.Find(Handle.Id))
			{
				OutTransform = *Found;
				return true;
			}
			return false;
		}

		virtual bool PersistAnchor(const FShiftAnchorHandle& Handle) override
		{
			// No cross-session persistence in the stub; report whether the anchor exists.
			return Anchors.Contains(Handle.Id);
		}

		virtual bool RaycastEnvironment(const UWorld* World, const FVector& Origin, const FVector& Direction, float Length, FShiftSceneHit& OutHit) const override
		{
			OutHit = FShiftSceneHit();
			if (!World)
			{
				return false;
			}

			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ShiftSceneRaycast), /*bTraceComplex=*/true);
			const FVector End = Origin + Direction.GetSafeNormal() * Length;
			const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, ECC_WorldStatic, Params);
			if (bHit)
			{
				OutHit.bHit = true;
				OutHit.Location = Hit.ImpactPoint;
				OutHit.Normal = Hit.ImpactNormal;
			}
			return bHit;
		}

		virtual bool ExportAnchorForSharing(const FShiftAnchorHandle& Handle, TArray<uint8>& OutBlob) const override
		{
			const FTransform* Found = Anchors.Find(Handle.Id);
			if (!Found)
			{
				return false;
			}
			OutBlob.Reset();
			FMemoryWriter Writer(OutBlob);
			FTransform T = *Found;
			Writer << T;
			return true;
		}

		virtual bool ImportSharedAnchor(const TArray<uint8>& Blob, FShiftAnchorHandle& OutHandle) override
		{
			if (Blob.Num() == 0)
			{
				return false;
			}
			FMemoryReader Reader(Blob);
			FTransform T;
			Reader << T;
			if (Reader.IsError() || T.ContainsNaN())
			{
				return false; // reject a corrupt / peer-supplied blob
			}
			return TryPlaceAnchor(T, OutHandle);
		}

		virtual const TCHAR* GetBackendName() const override { return TEXT("Stub"); }

	private:
		TMap<int32, FTransform> Anchors;
		int32 NextId = 0;
	};
}

UShiftXRSceneAnchor::UShiftXRSceneAnchor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UShiftXRSceneAnchor::BeginPlay()
{
	Super::BeginPlay();
	if (!Backend.IsValid())
	{
#if WITH_ANDROIDXR
		// Real Android XR backend (planes / anchors / persistence). Implemented once a UE 5.8-compatible
		// Android XR scene/anchor runtime exists — see ROADMAP.md "Engine-gap strategy"
		// (Track A: custom IOpenXRExtensionPlugin / Track B: port Google's 5.6 plugin). Flipping
		// WITH_ANDROIDXR on swaps the backend here; until the class exists we fall back to the stub.
		// Backend = MakeUnique<FShiftSceneBackend_AndroidXR>();
		Backend = MakeUnique<FShiftSceneBackend_Stub>();
		UE_LOG(LogShiftXR, Display, TEXT("[SceneAnchor] Backend = %s (WITH_ANDROIDXR build; real backend pending)."), Backend->GetBackendName());
#else
		Backend = MakeUnique<FShiftSceneBackend_Stub>();
		UE_LOG(LogShiftXR, Display, TEXT("[SceneAnchor] Backend = %s (stock UE 5.8 has no scene/anchor runtime)."), Backend->GetBackendName());
#endif
	}
}

bool UShiftXRSceneAnchor::TryPlaceAnchor(const FTransform& WorldTransform, FShiftAnchorHandle& OutHandle)
{
	if (!Backend.IsValid())
	{
		Backend = MakeUnique<FShiftSceneBackend_Stub>();
	}
	return Backend->TryPlaceAnchor(WorldTransform, OutHandle);
}

bool UShiftXRSceneAnchor::GetAnchorTransform(const FShiftAnchorHandle& Handle, FTransform& OutTransform) const
{
	return Backend.IsValid() && Backend->GetAnchorTransform(Handle, OutTransform);
}

bool UShiftXRSceneAnchor::PersistAnchor(const FShiftAnchorHandle& Handle)
{
	return Backend.IsValid() && Backend->PersistAnchor(Handle);
}

bool UShiftXRSceneAnchor::AnchorToSurfaceBelow(float MaxTraceDistance, FShiftAnchorHandle& OutHandle)
{
	if (!Backend.IsValid())
	{
		Backend = MakeUnique<FShiftSceneBackend_Stub>();
	}

	const FVector Origin = GetComponentLocation();
	FShiftSceneHit Hit;
	if (Backend->RaycastEnvironment(GetWorld(), Origin, FVector::DownVector, MaxTraceDistance, Hit))
	{
		const FTransform Surface(FRotationMatrix::MakeFromZ(Hit.Normal).Rotator(), Hit.Location);
		return Backend->TryPlaceAnchor(Surface, OutHandle);
	}

	// No surface found: anchor at the component's current transform.
	return Backend->TryPlaceAnchor(GetComponentTransform(), OutHandle);
}

bool UShiftXRSceneAnchor::RaycastEnvironment(const FVector& Origin, const FVector& Direction, float Length, FShiftSceneHit& OutHit) const
{
	return Backend.IsValid() && Backend->RaycastEnvironment(GetWorld(), Origin, Direction, Length, OutHit);
}

bool UShiftXRSceneAnchor::ExportAnchorForSharing(const FShiftAnchorHandle& Handle, TArray<uint8>& OutBlob) const
{
	return Backend.IsValid() && Backend->ExportAnchorForSharing(Handle, OutBlob);
}

bool UShiftXRSceneAnchor::ImportSharedAnchor(const TArray<uint8>& Blob, FShiftAnchorHandle& OutHandle)
{
	if (!Backend.IsValid())
	{
		Backend = MakeUnique<FShiftSceneBackend_Stub>();
	}
	return Backend->ImportSharedAnchor(Blob, OutHandle);
}

FString UShiftXRSceneAnchor::GetBackendName() const
{
	return Backend.IsValid() ? FString(Backend->GetBackendName()) : FString(TEXT("None"));
}
