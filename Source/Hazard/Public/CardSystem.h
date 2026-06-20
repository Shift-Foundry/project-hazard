#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HazardTypes.h"
#include "CardSystem.generated.h"

class UDataTable;

/** Supplies upgrade cards from DT_Cards (falls back to built-in defaults if absent). */
UCLASS()
class HAZARD_API UCardSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Return up to Count cards; RarityBias raises the odds of higher-rarity cards. */
	TArray<FCardRow> OfferCards(int32 Count, int32 RarityBias);

	static EItemRarity RollCardRarity(int32 RarityBias);

	UPROPERTY()
	TObjectPtr<UDataTable> CardTable = nullptr;

private:
	void EnsureTable();
	TArray<FCardRow> GetAllCards() const;
	static TArray<FCardRow> DefaultCards();
};
