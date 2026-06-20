#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HazardTypes.h"
#include "WeaponLibrary.generated.h"

class UDataTable;

/** Rolls weapon definitions by rarity from DT_Weapons (with built-in fallbacks). */
UCLASS()
class HAZARD_API UWeaponLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Weighted rarity roll for a milestone tier (higher tier + Luck -> better odds). */
	static EItemRarity RollRarityForTier(int32 MilestoneTier, int32 Luck = 0);

	/** UI color per rarity: Common gray, Uncommon green, Rare blue, Epic purple, Legendary yellow, Exclusive orange. */
	static FLinearColor RarityColor(EItemRarity Rarity);

	/** Pick a random weapon of (or near) the desired rarity. */
	static FWeaponDef RollWeaponDef(EItemRarity DesiredRarity);

	/** All weapons (DT_Weapons rows, else built-in defaults). */
	static TArray<FWeaponDef> GetAllWeapons();

	/** Weapons buyable in the shop (CoinCost>0 and not chest-only). */
	static TArray<FWeaponDef> GetShopWeapons();

	static FString RarityName(EItemRarity Rarity);

private:
	static UDataTable* GetWeaponTable();
	static TArray<FWeaponDef> DefaultWeapons();
};
