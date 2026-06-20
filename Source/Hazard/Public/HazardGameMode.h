#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "HazardGameMode.generated.h"

class UWaveDirector;
class AZombieBase;
class AChestActor;

/**
 * Server-only match controller. Owns the WaveDirector and feeds it the wave configuration
 * (set on BP_HazardGameMode by Python authoring). Default pawn = AHazardPlayerPawn,
 * GameState = AHazardGameState.
 */
UCLASS()
class HAZARD_API AHazardGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHazardGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UWaveDirector* WaveDirector;

	/** Wave config (mirrored into the WaveDirector at BeginPlay). Set on BP_HazardGameMode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Waves")
	TSubclassOf<AZombieBase> ZombieClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Waves")
	float SpawnRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Waves")
	float SpawnInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Waves")
	int32 BaseEnemiesPerWave = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Waves")
	int32 EnemiesPerWaveGrowth = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Waves")
	float TimeBetweenWaves = 5.f;

	/** Chest spawned when a player places one (set on BP_HazardGameMode; defaults to AChestActor). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hazard")
	TSubclassOf<AChestActor> ChestClass;
};
