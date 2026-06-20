#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ColocationComponent.generated.h"

/**
 * Co-op colocation: aligns the local player's tracking space to the shared room anchor
 * (GameState::SharedAnchorTransform) so two players in the same physical room map 1:1.
 *
 * On stock UE 5.8 there is no scene-anchor runtime (see ShiftXRSceneAnchor backend matrix),
 * so all clients already share replicated world coordinates and the alignment is identity.
 * The hook AlignToSharedAnchor() is where a real Android XR anchor backend would offset the
 * VR origin once a 5.8-compatible plugin is available.
 */
UCLASS(ClassGroup = (Hazard), meta = (BlueprintSpawnableComponent))
class HAZARD_API UColocationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UColocationComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Hazard|Coop")
	bool IsColocated() const { return bColocated; }

protected:
	void TryAlign();
	void AlignToSharedAnchor(const FTransform& SharedAnchor);

private:
	bool bColocated = false;
};
