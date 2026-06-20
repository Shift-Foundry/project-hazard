#include "HazardGameMode.h"
#include "WaveDirector.h"
#include "HazardGameState.h"
#include "HazardPlayerPawn.h"
#include "HazardHUD.h"
#include "ZombieBase.h"
#include "ChestActor.h"
#include "RunManager.h"
#include "HazardPlayerController.h"
#include "Engine/World.h"

AHazardGameMode::AHazardGameMode()
{
	WaveDirector = CreateDefaultSubobject<UWaveDirector>(TEXT("WaveDirector"));

	GameStateClass = AHazardGameState::StaticClass();
	DefaultPawnClass = AHazardPlayerPawn::StaticClass();
	PlayerControllerClass = AHazardPlayerController::StaticClass();
	HUDClass = AHazardHUD::StaticClass();

	// Default to the C++ enemy so waves run even before BP_Zombie is wired up.
	// BP_HazardGameMode overrides this with BP_Zombie (greybox mesh + enemy material).
	ZombieClass = AZombieBase::StaticClass();
	ChestClass = AChestActor::StaticClass();
}

void AHazardGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (WaveDirector)
	{
		WaveDirector->ZombieClass = ZombieClass;
		WaveDirector->SpawnRadius = SpawnRadius;
		WaveDirector->SpawnInterval = SpawnInterval;
		WaveDirector->BaseEnemiesPerWave = BaseEnemiesPerWave;
		WaveDirector->EnemiesPerWaveGrowth = EnemiesPerWaveGrowth;
		WaveDirector->TimeBetweenWaves = TimeBetweenWaves;
	}

	// The RunManager drives the level -> card -> level loop (it starts the WaveDirector).
	if (UWorld* W = GetWorld())
	{
		if (URunManager* RM = W->GetSubsystem<URunManager>())
		{
			RM->StartRun();
		}
	}
}

void AHazardGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogTemp, Display, TEXT("[Hazard] Player joined — %d player(s) in the room."), GetNumPlayers());
}

void AHazardGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	UE_LOG(LogTemp, Display, TEXT("[Hazard] Player left the co-op session."));
}
