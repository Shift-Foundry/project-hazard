#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "HazardTypes.h"
#include "WaveDirector.generated.h"

class AZombieBase;
class AHazardBase;
class AHazardGameState;
class UShiftXRPerfGovernor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllWavesCleared);

/**
 * Server-authoritative wave spawner on the GameMode. The RunManager drives it one level at a
 * time via StartLevel(); when all of a level's waves are cleared it broadcasts OnAllWavesCleared.
 * Spawns an escalating archetype mix on a ring around the Base, honoring the PerfGovernor budget.
 */
UCLASS(ClassGroup = (Hazard), meta = (BlueprintSpawnableComponent))
class HAZARD_API UWaveDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaveDirector();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TSubclassOf<AZombieBase> ZombieClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "50.0"))
	float SpawnRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.05"))
	float SpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "1"))
	int32 BaseEnemiesPerWave = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	int32 EnemiesPerWaveGrowth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0"))
	float TimeBetweenWaves = 5.f;

	/** Waves in level N = WavesPerLevelBase + N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "1"))
	int32 WavesPerLevelBase = 2;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnAllWavesCleared OnAllWavesCleared;

	/** True if a boss appeared during the level just completed (raises card rarity odds). */
	UPROPERTY(BlueprintReadOnly, Category = "Waves")
	bool bBossSpawnedThisLevel = false;

	/** Server: run one level's worth of waves with the given difficulty multiplier. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StartLevel(int32 Level, float DifficultyMul);

	/** Server: stop spawning (e.g., on game over). */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StopWaves();

private:
	bool bRunning = false;
	bool bWaveActive = false;
	int32 GlobalWave = 0;
	int32 LevelIndex = 0;
	int32 WavesThisLevel = 0;
	int32 WavesDoneThisLevel = 0;
	float DifficultyMul = 1.f;
	int32 EnemiesToSpawnThisWave = 0;
	int32 EnemiesSpawnedThisWave = 0;
	float TimeSinceSpawn = 0.f;
	float InterWaveTimer = 0.f;

	UPROPERTY()
	TArray<TWeakObjectPtr<AZombieBase>> LiveZombies;

	void BeginNextWave();
	void TickWaveLogic(float DeltaTime);
	void TrySpawnOne();
	void SpawnBoss();
	int32 CountAlive();
	FVector PickSpawnLocation() const;
	EZombieArchetype PickArchetype() const;
	AActor* GetZombieTarget() const;
	AHazardBase* FindBase() const;
	UShiftXRPerfGovernor* GetPerfGovernor() const;
	AHazardGameState* GetHazardGameState() const;
	void PushStateToGameState();
};
