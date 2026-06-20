#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HazardTypes.h"
#include "HazardGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHazardWaveChanged, int32, NewWave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHazardRunStateChanged, ERunState, NewState);

/** Replicated match state visible to all clients (GameMode lives on the server only). */
UCLASS()
class HAZARD_API AHazardGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Wave, BlueprintReadOnly, Category = "Hazard")
	int32 CurrentWave = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	int32 EnemiesAlive = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	int32 EnemiesRemainingInWave = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	float BaseHealthPercent = 1.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	int32 Score = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	int32 Coins = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RunState, BlueprintReadOnly, Category = "Hazard")
	ERunState RunState = ERunState::NotStarted;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	int32 CurrentLevel = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	int32 HighScore = 0;

	/** Display names of the 3 cards currently offered (empty unless ChoosingCard). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	TArray<FString> OfferedCards;

	/** Rarity (EItemRarity as uint8) per offered card, parallel to OfferedCards. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard")
	TArray<uint8> OfferedCardRarities;

	// --- In-run weapon RNG roller (top-of-interface; rerolls every ~10s during Playing) ---
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Roller")
	FString RollerWeaponName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Roller")
	uint8 RollerWeaponRarity = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Roller")
	bool bRollerActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Roller")
	bool bRollerClaimed = false;

	/** Server world-time at which the current offer rerolls (drives the HUD countdown). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Roller")
	float RollerEndTime = 0.f;

	/** Co-op colocation: the shared real-world reference frame all players align to. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Coop")
	FTransform SharedAnchorTransform = FTransform::Identity;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Coop")
	bool bColocationReady = false;

	UPROPERTY(BlueprintAssignable, Category = "Hazard")
	FOnHazardWaveChanged OnWaveChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hazard")
	FOnHazardRunStateChanged OnRunStateChanged;

	/** Server: set the wave index and broadcast locally (clients update via OnRep_Wave). */
	void SetCurrentWave(int32 NewWave);

	/** Server: add points to the run score. */
	void AddScore(int32 Points);

	/** Server: add run currency. */
	void AddCoins(int32 Amount);

	/** Server: set run state and broadcast locally (clients update via OnRep_RunState). */
	void SetRunState(ERunState NewState);

protected:
	UFUNCTION()
	void OnRep_Wave();

	UFUNCTION()
	void OnRep_RunState();
};
