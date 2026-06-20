#include "ColocationComponent.h"
#include "HazardGameState.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UColocationComponent::UColocationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UColocationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bColocated)
	{
		TryAlign();
	}
}

void UColocationComponent::TryAlign()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return; // each client colocates its own view
	}

	const AHazardGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHazardGameState>() : nullptr;
	if (!GS || !GS->bColocationReady)
	{
		return;
	}

	AlignToSharedAnchor(GS->SharedAnchorTransform);
	bColocated = true;
}

void UColocationComponent::AlignToSharedAnchor(const FTransform& SharedAnchor)
{
#if WITH_ANDROIDXR
	// Real backend: resolve the local instance of the shared physical anchor (imported via
	// UShiftXRSceneAnchor::ImportSharedAnchor) and offset this pawn's VR origin so SharedAnchor
	// maps to the same physical point on every client. Pending the UE 5.8 Android XR anchor API
	// (see ROADMAP.md "Engine-gap strategy"). TODO: compute + apply the VR-origin offset here.
	UE_LOG(LogTemp, Display, TEXT("[Coop] %s colocation via shared physical anchor (WITH_ANDROIDXR; offset impl pending)."),
		*GetOwner()->GetName());
#else
	// Stub backend: clients already share replicated world coordinates, so alignment is identity
	// and we simply mark colocation ready.
	UE_LOG(LogTemp, Display, TEXT("[Coop] %s colocated to shared anchor at %s (stub: identity)."),
		*GetOwner()->GetName(), *SharedAnchor.GetLocation().ToString());
#endif
}
