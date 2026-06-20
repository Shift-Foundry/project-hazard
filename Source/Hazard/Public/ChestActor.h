#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "ChestActor.generated.h"

class UStaticMeshComponent;

/**
 * Player-placeable / milestone chest. Opens on a timer (server) and case-rolls a reward:
 * a rarity weapon (from the unlocked pool via URunManager) or a coin payout. Replicated; open FX multicast.
 */
UCLASS()
class HAZARD_API AChestActor : public AActor
{
	GENERATED_BODY()

public:
	AChestActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds after placement before the chest opens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest", meta = (ClampMin = "0.0"))
	float OpenDelay = 8.f;

	/** Milestone tier (1..5): higher = better loot odds. Set by the spawner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest")
	int32 RarityTier = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest")
	UStaticMeshComponent* MeshComp;

	UFUNCTION(BlueprintPure, Category = "Chest")
	bool IsOpened() const { return bOpened; }

protected:
	UFUNCTION()
	void Open();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOpenFX();

	UPROPERTY(ReplicatedUsing = OnRep_Opened)
	bool bOpened = false;

	UFUNCTION()
	void OnRep_Opened();

private:
	FTimerHandle OpenHandle;
	FVector BaseScale = FVector(1.f);
};
