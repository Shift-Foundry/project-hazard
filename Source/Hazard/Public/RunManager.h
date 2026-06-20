#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "HazardTypes.h"
#include "RunManager.generated.h"

class UWaveDirector;
class AHazardGameState;
class UCardSystem;
class AHazardPlayerPawn;

/**
 * Server-authoritative roguelite orchestrator (per world). Drives the level -> clear -> card
 * -> harder level loop, holds stackable run modifiers queried by weapons, applies loot grants,
 * and persists the high score via UHazardSaveGame. Run state mirrors to clients via GameState.
 */
UCLASS()
class HAZARD_API URunManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server: begin a fresh run (called by the GameMode). */
	UFUNCTION(BlueprintCallable, Category = "Hazard")
	void StartRun();

	/** Server: choose one of the currently-offered cards (0..2). */
	UFUNCTION(BlueprintCallable, Category = "Hazard")
	void PickCard(int32 Index);

	/** Server: end the run, persist records. */
	UFUNCTION(BlueprintCallable, Category = "Hazard")
	void EndRun();

	/** Server: persist records WITHOUT ending the run (lifecycle: app backgrounded / headset removed). */
	void SaveProgress();

	UFUNCTION(BlueprintPure, Category = "Hazard")
	ERunState GetRunState() const { return RunState; }

	UFUNCTION(BlueprintPure, Category = "Hazard")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	UFUNCTION(BlueprintPure, Category = "Hazard")
	int32 GetHighScore() const { return HighScore; }

	// Run modifiers (queried by AWeaponBase at fire time).
	float GetDamageMult() const { return DamageMult; }
	float GetFireRateMult() const { return FireRateMult; }
	float GetMeleeDamageMult() const { return MeleeDamageMult; }
	float GetProjectileSpeedMult() const { return ProjectileSpeedMult; }
	int32 GetBonusAmmo() const { return BonusAmmo; }

	UFUNCTION(BlueprintPure, Category = "Hazard")
	int32 GetLuck() const { return Luck; }

	/** Server: a player pawn died; ends the run if all players are down. */
	void NotifyPlayerDied();

	/** Server: a player claimed the current weapon-roller offer -> equip it into their arsenal. */
	void ClaimRolledWeapon(AHazardPlayerPawn* Pawn);

	/** Roll a weapon of (or near) the desired rarity from the unlocked pool + free starters.
	 *  bAllowChestOnly=true also admits chest-only Exclusives (used by milestone chests). */
	FWeaponDef RollUnlockedWeapon(EItemRarity DesiredRarity, bool bAllowChestOnly = false) const;

protected:
	UFUNCTION()
	void HandleAllWavesCleared();

	void BeginNextLevel();
	void OfferCards();
	void RollWeaponOffer();
	void AutoPickNow();
	void ApplyCardEffect(const FCardRow& Card);
	void ApplyPlayerStatCards();
	void SetState(ERunState NewState);
	void PushToGameState();
	void LoadRecords();
	void SaveRecordsIfBest();
	/** Bank any newly-earned run coins (delta since last commit) + update records. Idempotent. */
	void CommitRunCoins();
	void EstablishSharedAnchor();
	int32 CountAlivePlayers() const;

	UWaveDirector* GetWaveDirector() const;
	AHazardGameState* GetGS() const;
	UCardSystem* GetCardSystem() const;

private:
	ERunState RunState = ERunState::NotStarted;
	int32 CurrentLevel = 0;
	int32 HighScore = 0;
	int32 BestLevel = 0;

	float DamageMult = 1.f;
	float FireRateMult = 1.f;
	float MeleeDamageMult = 1.f;
	float ProjectileSpeedMult = 1.f;
	float BonusMaxHealth = 0.f;
	float ClassHealthBonus = 0.f; // from the selected class; base for card health recompute
	int32 BonusAmmo = 0;
	int32 Luck = 0;
	int32 CoinsBankedThisRun = 0; // delta-bank tracker so coins bank once (run-end + lifecycle)

	bool bBoundWaves = false;
	TArray<FCardRow> CurrentOffer;
	FTimerHandle AutoPickHandle;

	// In-run weapon RNG roller
	FTimerHandle RollerHandle;
	TArray<FName> UnlockedWeaponRows;
	FWeaponDef CurrentRollerDef;
};
