#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HazardSaveGame.generated.h"

/** Persistent profile: run records + meta-progression (coin wallet, unlocks, selected class). */
UCLASS()
class HAZARD_API UHazardSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 HighScore = 0;

	UPROPERTY()
	int32 BestLevel = 0;

	/** Persistent coin wallet, spent in the lobby shop. */
	UPROPERTY()
	int32 CoinsBanked = 0;

	/** Weapon DT row names the player has unlocked into the in-run RNG pool. */
	UPROPERTY()
	TArray<FName> UnlockedWeapons;

	/** Class DT row names the player has unlocked. */
	UPROPERTY()
	TArray<FName> UnlockedClasses;

	/** Currently selected class row (applied at run start). */
	UPROPERTY()
	FName SelectedClass = NAME_None;
};
