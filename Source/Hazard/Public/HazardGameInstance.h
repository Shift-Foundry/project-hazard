#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HazardGameInstance.generated.h"

/** Persists cross-level choices (difficulty) selected in the menu into the gameplay level. */
UCLASS()
class HAZARD_API UHazardGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Hazard")
	int32 DifficultyIndex = 1; // 0 Easy, 1 Normal, 2 Hard

	UPROPERTY(BlueprintReadOnly, Category = "Hazard")
	float DifficultyScale = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Hazard")
	void SetDifficultyIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Hazard")
	FString GetDifficultyName() const;

	// --- Persistent profile (meta-progression: coin wallet, unlocks, selected class) ---
	int32 GetCoins() const { return CoinsBanked; }
	int32 GetHighScore() const { return HighScore; }
	int32 GetBestLevel() const { return BestLevel; }
	FName GetSelectedClass() const { return SelectedClass; }
	const TArray<FName>& GetUnlockedWeapons() const { return UnlockedWeapons; }
	const TArray<FName>& GetUnlockedClasses() const { return UnlockedClasses; }
	bool IsWeaponUnlocked(FName Row) const { return UnlockedWeapons.Contains(Row); }
	bool IsClassUnlocked(FName Row) const { return UnlockedClasses.Contains(Row); }

	/** Lobby shop: spend banked coins to unlock a weapon/class. Returns true on success. */
	bool TryBuyWeapon(FName Row, int32 Cost);
	bool TryBuyClass(FName Row, int32 Cost);
	bool SetSelectedClass(FName Row);

	/** Run end: add earned coins + update records, then persist. */
	void BankRunResult(int32 CoinsEarned, int32 Score, int32 Level);
	/** Update high score / best level only (no coin change), then persist. */
	void UpdateRecords(int32 Score, int32 Level);

	void SaveProfile();

	virtual void Init() override;
	virtual void Shutdown() override;

private:
	// XR/mobile app-lifecycle: pause + persist the run when focus is lost (system menu,
	// headset removed, app backgrounded) so the in-progress run isn't lost to an OS kill.
	void HandleAppDeactivate();
	void HandleAppReactivate();

	FDelegateHandle DeactivateHandle;
	FDelegateHandle ReactivateHandle;
	bool bPausedByLifecycle = false; // only auto-unpause on resume what we auto-paused on focus loss

	void LoadProfile();
	void EnsureDefaults();

	int32 CoinsBanked = 0;
	int32 HighScore = 0;
	int32 BestLevel = 0;
	TArray<FName> UnlockedWeapons;
	TArray<FName> UnlockedClasses;
	FName SelectedClass = NAME_None;
};
