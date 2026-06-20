#include "WaveDirector.h"
#include "ZombieBase.h"
#include "HazardBase.h"
#include "HazardGameState.h"
#include "HealthComponent.h"
#include "ShiftXRPerfGovernor.h"
#include "ShiftXRCore.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarBossEvery(
	TEXT("hazard.BossEvery"), 10,
	TEXT("Spawn a boss every N waves (default 10)."));

UWaveDirector::UWaveDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWaveDirector::BeginPlay()
{
	Super::BeginPlay();
}

void UWaveDirector::StartLevel(int32 Level, float InDifficultyMul)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	LevelIndex = Level;
	DifficultyMul = InDifficultyMul;
	WavesThisLevel = FMath::Max(1, WavesPerLevelBase + Level);
	WavesDoneThisLevel = 0;
	bWaveActive = false;
	bRunning = true;
	bBossSpawnedThisLevel = false;
	InterWaveTimer = TimeBetweenWaves; // start first wave promptly
	UE_LOG(LogShiftXR, Display, TEXT("[WaveDirector] Level %d: %d waves, difficulty x%.2f"), Level, WavesThisLevel, DifficultyMul);
}

void UWaveDirector::StopWaves()
{
	bRunning = false;
	bWaveActive = false;
}

void UWaveDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRunning || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TickWaveLogic(DeltaTime);
	PushStateToGameState();
}

void UWaveDirector::TickWaveLogic(float DeltaTime)
{
	if (!bWaveActive)
	{
		InterWaveTimer += DeltaTime;
		if (InterWaveTimer >= TimeBetweenWaves)
		{
			BeginNextWave();
		}
		return;
	}

	TimeSinceSpawn += DeltaTime;

	const int32 Alive = CountAlive();
	const UShiftXRPerfGovernor* Gov = GetPerfGovernor();
	const int32 Budget = Gov ? Gov->GetMaxConcurrentEnemies() : 20;

	if (EnemiesSpawnedThisWave < EnemiesToSpawnThisWave
		&& TimeSinceSpawn >= SpawnInterval
		&& Alive < Budget)
	{
		TimeSinceSpawn = 0.f;
		TrySpawnOne();
	}

	if (EnemiesSpawnedThisWave >= EnemiesToSpawnThisWave && CountAlive() == 0)
	{
		++WavesDoneThisLevel;
		bWaveActive = false;
		InterWaveTimer = 0.f;
		UE_LOG(LogShiftXR, Display, TEXT("[WaveDirector] Wave %d cleared (%d/%d this level)."), GlobalWave, WavesDoneThisLevel, WavesThisLevel);

		if (WavesDoneThisLevel >= WavesThisLevel)
		{
			bRunning = false;
			UE_LOG(LogShiftXR, Display, TEXT("[WaveDirector] Level %d complete."), LevelIndex);
			OnAllWavesCleared.Broadcast();
		}
	}
}

void UWaveDirector::BeginNextWave()
{
	++GlobalWave;
	const int32 Raw = BaseEnemiesPerWave + WavesDoneThisLevel * EnemiesPerWaveGrowth;
	EnemiesToSpawnThisWave = FMath::Max(1, FMath::RoundToInt(Raw * DifficultyMul));
	EnemiesSpawnedThisWave = 0;
	TimeSinceSpawn = SpawnInterval;
	bWaveActive = true;

	UE_LOG(LogShiftXR, Display, TEXT("[WaveDirector] Wave %d begins: %d enemies."), GlobalWave, EnemiesToSpawnThisWave);

	if (AHazardGameState* GS = GetHazardGameState())
	{
		GS->SetCurrentWave(GlobalWave);
	}

	// Every Nth wave is a boss wave: spawn a boss in addition to the adds.
	const int32 BossEvery = FMath::Max(1, CVarBossEvery.GetValueOnGameThread());
	if (GlobalWave % BossEvery == 0)
	{
		SpawnBoss();
		UE_LOG(LogShiftXR, Display, TEXT("[WaveDirector] BOSS WAVE %d!"), GlobalWave);
	}
}

void UWaveDirector::SpawnBoss()
{
	if (!ZombieClass)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector SpawnLoc = PickSpawnLocation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();

	if (AZombieBase* Boss = World->SpawnActor<AZombieBase>(ZombieClass, SpawnLoc, FRotator::ZeroRotator, Params))
	{
		Boss->SetArchetype(EZombieArchetype::Brute);
		Boss->SetBoss(FMath::Max(1, GlobalWave / 10));
		Boss->SetTargetActor(GetZombieTarget());
		LiveZombies.Add(Boss);
		bBossSpawnedThisLevel = true;
	}
}

void UWaveDirector::TrySpawnOne()
{
	if (!ZombieClass)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector SpawnLoc = PickSpawnLocation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();

	if (AZombieBase* Zombie = World->SpawnActor<AZombieBase>(ZombieClass, SpawnLoc, FRotator::ZeroRotator, Params))
	{
		Zombie->SetArchetype(PickArchetype());
		if (DifficultyMul > 1.f && Zombie->Health)
		{
			Zombie->Health->MaxHealth *= DifficultyMul;
			Zombie->Health->ResetToFull();
		}
		Zombie->SetTargetActor(GetZombieTarget());
		LiveZombies.Add(Zombie);
		++EnemiesSpawnedThisWave;
	}
}

int32 UWaveDirector::CountAlive()
{
	int32 Count = 0;
	for (int32 i = LiveZombies.Num() - 1; i >= 0; --i)
	{
		AZombieBase* Z = LiveZombies[i].Get();
		if (!Z)
		{
			LiveZombies.RemoveAtSwap(i);
			continue;
		}
		// Dead-but-not-yet-destroyed zombies (death-FX lifespan) must not count as alive,
		// or they stall wave completion and eat the concurrent-spawn budget.
		if (Z->Health && Z->Health->IsDead())
		{
			continue;
		}
		++Count;
	}
	return Count;
}

FVector UWaveDirector::PickSpawnLocation() const
{
	FVector Center = FVector::ZeroVector;
	if (const AHazardBase* Base = FindBase())
	{
		Center = Base->GetActorLocation();
	}

	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Offset(FMath::Cos(Angle) * SpawnRadius, FMath::Sin(Angle) * SpawnRadius, 88.f);
	return Center + Offset;
}

EZombieArchetype UWaveDirector::PickArchetype() const
{
	const float Roll = FMath::FRand();
	if (GlobalWave >= 3)
	{
		if (Roll < 0.20f) return EZombieArchetype::Brute;
		if (Roll < 0.50f) return EZombieArchetype::Runner;
		return EZombieArchetype::Walker;
	}
	if (GlobalWave == 2)
	{
		return (Roll < 0.30f) ? EZombieArchetype::Runner : EZombieArchetype::Walker;
	}
	return EZombieArchetype::Walker;
}

AActor* UWaveDirector::GetZombieTarget() const
{
	return FindBase();
}

AHazardBase* UWaveDirector::FindBase() const
{
	for (TActorIterator<AHazardBase> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

UShiftXRPerfGovernor* UWaveDirector::GetPerfGovernor() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UShiftXRPerfGovernor>();
	}
	return nullptr;
}

AHazardGameState* UWaveDirector::GetHazardGameState() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetGameState<AHazardGameState>();
	}
	return nullptr;
}

void UWaveDirector::PushStateToGameState()
{
	AHazardGameState* GS = GetHazardGameState();
	if (!GS)
	{
		return;
	}

	const int32 Alive = CountAlive();
	GS->EnemiesAlive = Alive;
	GS->EnemiesRemainingInWave = FMath::Max(0, EnemiesToSpawnThisWave - EnemiesSpawnedThisWave) + Alive;

	if (const AHazardBase* Base = FindBase())
	{
		if (const UHealthComponent* H = Base->GetHealthComponent())
		{
			GS->BaseHealthPercent = H->GetHealthPercent();
		}
	}
}
