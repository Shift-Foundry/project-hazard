#include "WeaponLibrary.h"
#include "Engine/DataTable.h"

UDataTable* UWeaponLibrary::GetWeaponTable()
{
	return LoadObject<UDataTable>(nullptr, TEXT("/Game/Hazard/Data/DT_Weapons.DT_Weapons"));
}

TArray<FWeaponDef> UWeaponLibrary::DefaultWeapons()
{
	TArray<FWeaponDef> W;
	auto Add = [&W](const TCHAR* Name, EWeaponMode Mode, EItemRarity Rarity, float Dmg, float Interval,
		int32 Ammo, int32 Cost, bool bChestOnly, bool bCrit)
	{
		FWeaponDef D;
		D.RowName = FName(Name);
		D.DisplayName = Name;
		D.Mode = Mode;
		D.Rarity = Rarity;
		D.Damage = Dmg;
		D.FireInterval = Interval;
		D.MaxAmmo = Ammo;
		D.CoinCost = Cost;
		D.bChestOnly = bChestOnly;
		if (bCrit) { D.Affixes.Add(EAffix::Crit); }
		W.Add(D);
	};
	//   Name            Mode                         Rarity                  Dmg   Int   Ammo Cost  ChestOnly Crit
	Add(TEXT("Pistol"),    EWeaponMode::RangedHitscan,    EItemRarity::Common,    14,  0.22f, 12,  0,   false, false);
	Add(TEXT("Blade"),     EWeaponMode::Melee,            EItemRarity::Common,    35,  0.55f,  0,  0,   false, false);
	Add(TEXT("MP5"),       EWeaponMode::RangedHitscan,    EItemRarity::Uncommon,  11,  0.09f, 30, 150,  false, false); // fast SMG
	Add(TEXT("Shotgun"),   EWeaponMode::RangedHitscan,    EItemRarity::Rare,      46,  0.75f,  8, 280,  false, false); // burst dmg
	Add(TEXT("M4"),        EWeaponMode::RangedHitscan,    EItemRarity::Rare,      18,  0.12f, 30, 320,  false, false); // full-auto rifle
	Add(TEXT("SKS"),       EWeaponMode::RangedHitscan,    EItemRarity::Epic,      30,  0.14f, 20, 520,  false, true);  // fast-firing marksman
	Add(TEXT("Cannon"),    EWeaponMode::RangedProjectile, EItemRarity::Legendary, 75,  0.80f,  6, 950,  false, true);
	Add(TEXT("VoidLance"), EWeaponMode::RangedProjectile, EItemRarity::Exclusive, 95,  0.42f, 10,  0,   true,  true);
	return W;
}

TArray<FWeaponDef> UWeaponLibrary::GetAllWeapons()
{
	TArray<FWeaponDef> Out;
	if (UDataTable* DT = GetWeaponTable())
	{
		// Iterate by row name so each weapon keeps a stable RowName identity (GetAllRows drops names).
		for (const FName& RowName : DT->GetRowNames())
		{
			if (const FWeaponDef* R = DT->FindRow<FWeaponDef>(RowName, TEXT("WeaponLibrary")))
			{
				FWeaponDef D = *R;
				if (D.RowName.IsNone()) { D.RowName = RowName; }
				Out.Add(D);
			}
		}
	}
	if (Out.Num() == 0)
	{
		Out = DefaultWeapons();
	}
	return Out;
}

TArray<FWeaponDef> UWeaponLibrary::GetShopWeapons()
{
	TArray<FWeaponDef> Out;
	for (const FWeaponDef& D : GetAllWeapons())
	{
		if (D.CoinCost > 0 && !D.bChestOnly && D.Rarity != EItemRarity::Exclusive)
		{
			Out.Add(D);
		}
	}
	return Out;
}

EItemRarity UWeaponLibrary::RollRarityForTier(int32 MilestoneTier, int32 Luck)
{
	const int32 Tier = FMath::Clamp(MilestoneTier, 1, 5);
	const float L = FMath::Clamp(FMath::Max(0, Luck) * 0.02f, 0.f, 0.6f); // luck shifts weight up (clamped)

	// Weighted table sampled once: monotonic + bound-safe, and every rarity stays reachable
	// (the old cascade let a >1.0 threshold swallow lower tiers; Luck was also unclamped).
	float W[6];
	W[static_cast<int32>(EItemRarity::Common)]    = FMath::Max(0.1f, 6.f - 1.0f * Tier - 4.0f * L);
	W[static_cast<int32>(EItemRarity::Uncommon)]  = 4.0f;
	W[static_cast<int32>(EItemRarity::Rare)]      = 1.0f + 0.6f * Tier + 2.0f * L;
	W[static_cast<int32>(EItemRarity::Epic)]      = 0.4f + 0.4f * Tier + 1.5f * L;
	W[static_cast<int32>(EItemRarity::Legendary)] = 0.15f + 0.25f * Tier + 1.0f * L;
	W[static_cast<int32>(EItemRarity::Exclusive)] = (Tier >= 3) ? (0.05f * (Tier - 2) + 0.5f * L) : 0.f;

	float Sum = 0.f;
	for (float x : W) { Sum += x; }
	float Pick = FMath::FRand() * Sum;
	for (int32 i = 0; i < 6; ++i)
	{
		if (Pick < W[i]) { return static_cast<EItemRarity>(i); }
		Pick -= W[i];
	}
	return EItemRarity::Common;
}

FLinearColor UWeaponLibrary::RarityColor(EItemRarity Rarity)
{
	switch (Rarity)
	{
	case EItemRarity::Uncommon:  return FLinearColor(0.45f, 0.90f, 0.45f); // green
	case EItemRarity::Rare:      return FLinearColor(0.30f, 0.55f, 1.00f); // blue
	case EItemRarity::Epic:      return FLinearColor(0.70f, 0.30f, 1.00f); // purple
	case EItemRarity::Legendary: return FLinearColor(1.00f, 0.85f, 0.20f); // yellow
	case EItemRarity::Exclusive: return FLinearColor(1.00f, 0.50f, 0.10f); // orange
	default:                     return FLinearColor(0.75f, 0.75f, 0.75f); // gray (Common)
	}
}

FWeaponDef UWeaponLibrary::RollWeaponDef(EItemRarity DesiredRarity)
{
	const TArray<FWeaponDef> All = GetAllWeapons();

	// Exact-rarity matches first.
	TArray<FWeaponDef> Matches;
	for (const FWeaponDef& D : All)
	{
		if (D.Rarity == DesiredRarity)
		{
			Matches.Add(D);
		}
	}
	// Fall back to the closest lower rarity that has any weapon.
	for (int32 R = static_cast<int32>(DesiredRarity); R >= 0 && Matches.Num() == 0; --R)
	{
		for (const FWeaponDef& D : All)
		{
			if (static_cast<int32>(D.Rarity) == R)
			{
				Matches.Add(D);
			}
		}
	}
	// Then upward (a missing low tier should still resolve to a real weapon, not a blank default).
	for (int32 R = static_cast<int32>(DesiredRarity) + 1; R <= static_cast<int32>(EItemRarity::Exclusive) && Matches.Num() == 0; ++R)
	{
		for (const FWeaponDef& D : All)
		{
			if (static_cast<int32>(D.Rarity) == R)
			{
				Matches.Add(D);
			}
		}
	}
	if (Matches.Num() == 0)
	{
		return All.Num() > 0 ? All[0] : FWeaponDef();
	}
	return Matches[FMath::RandRange(0, Matches.Num() - 1)];
}

FString UWeaponLibrary::RarityName(EItemRarity Rarity)
{
	switch (Rarity)
	{
	case EItemRarity::Uncommon:  return TEXT("Uncommon");
	case EItemRarity::Rare:      return TEXT("Rare");
	case EItemRarity::Epic:      return TEXT("Epic");
	case EItemRarity::Legendary: return TEXT("Legendary");
	case EItemRarity::Exclusive: return TEXT("Exclusive");
	default:                     return TEXT("Common");
	}
}
