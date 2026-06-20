#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HazardBase.generated.h"

class UStaticMeshComponent;
class UShiftXRSceneAnchor;
class UHealthComponent;

/**
 * The objective the player defends. Anchored to a real surface via UShiftXRSceneAnchor
 * (stub backend on UE 5.8) and carries replicated health. Single swappable mesh slot
 * (SM_GB_Base). Replicated and server-authoritative.
 */
UCLASS()
class HAZARD_API AHazardBase : public AActor
{
	GENERATED_BODY()

public:
	AHazardBase();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	USceneComponent* SceneRoot;

	/** Single swappable greybox slot (SM_GB_Base). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UShiftXRSceneAnchor* SceneAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UHealthComponent* Health;

	UFUNCTION(BlueprintPure, Category = "Hazard")
	UHealthComponent* GetHealthComponent() const { return Health; }

	/** Max distance to search downward for a surface to anchor onto. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard")
	float AnchorTraceDistance = 500.f;

protected:
	UFUNCTION()
	void HandleBaseDestroyed(AActor* Killer);
};
