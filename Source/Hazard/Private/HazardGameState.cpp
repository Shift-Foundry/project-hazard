#include "HazardGameState.h"
#include "Net/UnrealNetwork.h"

void AHazardGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHazardGameState, CurrentWave);
	DOREPLIFETIME(AHazardGameState, EnemiesAlive);
	DOREPLIFETIME(AHazardGameState, EnemiesRemainingInWave);
	DOREPLIFETIME(AHazardGameState, BaseHealthPercent);
	DOREPLIFETIME(AHazardGameState, Score);
	DOREPLIFETIME(AHazardGameState, Coins);
	DOREPLIFETIME(AHazardGameState, RunState);
	DOREPLIFETIME(AHazardGameState, CurrentLevel);
	DOREPLIFETIME(AHazardGameState, HighScore);
	DOREPLIFETIME(AHazardGameState, OfferedCards);
	DOREPLIFETIME(AHazardGameState, OfferedCardRarities);
	DOREPLIFETIME(AHazardGameState, RollerWeaponName);
	DOREPLIFETIME(AHazardGameState, RollerWeaponRarity);
	DOREPLIFETIME(AHazardGameState, bRollerActive);
	DOREPLIFETIME(AHazardGameState, bRollerClaimed);
	DOREPLIFETIME(AHazardGameState, RollerEndTime);
	DOREPLIFETIME(AHazardGameState, SharedAnchorTransform);
	DOREPLIFETIME(AHazardGameState, bColocationReady);
}

void AHazardGameState::AddScore(int32 Points)
{
	Score += Points;
}

void AHazardGameState::AddCoins(int32 Amount)
{
	Coins = FMath::Max(0, Coins + Amount);
}

void AHazardGameState::SetRunState(ERunState NewState)
{
	RunState = NewState;
	OnRunStateChanged.Broadcast(RunState); // server-side; clients via OnRep_RunState
}

void AHazardGameState::OnRep_RunState()
{
	OnRunStateChanged.Broadcast(RunState);
}

void AHazardGameState::SetCurrentWave(int32 NewWave)
{
	CurrentWave = NewWave;
	// Server-side broadcast; remote clients are notified through OnRep_Wave.
	OnWaveChanged.Broadcast(CurrentWave);
}

void AHazardGameState::OnRep_Wave()
{
	OnWaveChanged.Broadcast(CurrentWave);
}
