#include "CardSystem.h"
#include "Engine/DataTable.h"

void UCardSystem::EnsureTable()
{
	if (!CardTable)
	{
		CardTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Hazard/Data/DT_Cards.DT_Cards"));
	}
}

TArray<FCardRow> UCardSystem::DefaultCards()
{
	TArray<FCardRow> C;
	auto Add = [&C](const TCHAR* Name, const TCHAR* Desc, ECardEffect E, float M, EItemRarity R)
	{
		FCardRow Row;
		Row.DisplayName = Name;
		Row.Description = Desc;
		Row.Effect = E;
		Row.Magnitude = M;
		Row.Rarity = R;
		C.Add(Row);
	};
	// Common (gray)
	Add(TEXT("Sharpshooter"), TEXT("+12% weapon damage"), ECardEffect::WeaponDamageUp, 0.12f, EItemRarity::Common);
	Add(TEXT("Rapid Fire"), TEXT("+15% fire rate"), ECardEffect::FireRateUp, 0.15f, EItemRarity::Common);
	Add(TEXT("Heavy Blade"), TEXT("+25% melee damage"), ECardEffect::MeleeDamageUp, 0.25f, EItemRarity::Common);
	Add(TEXT("Vitality"), TEXT("+20 max health"), ECardEffect::MaxHealthUp, 20.f, EItemRarity::Common);
	Add(TEXT("Extended Mag"), TEXT("+4 ammo"), ECardEffect::AmmoCapacityUp, 4.f, EItemRarity::Common);
	Add(TEXT("Lucky Charm"), TEXT("Luck +1 (better drops)"), ECardEffect::LuckUp, 1.f, EItemRarity::Common);
	// Rare (blue)
	Add(TEXT("Marksman"), TEXT("+25% weapon damage"), ECardEffect::WeaponDamageUp, 0.25f, EItemRarity::Rare);
	Add(TEXT("Overclock"), TEXT("+30% fire rate"), ECardEffect::FireRateUp, 0.30f, EItemRarity::Rare);
	Add(TEXT("Fortified"), TEXT("+40 max health"), ECardEffect::MaxHealthUp, 40.f, EItemRarity::Rare);
	Add(TEXT("Four-Leaf"), TEXT("Luck +2"), ECardEffect::LuckUp, 2.f, EItemRarity::Rare);
	// Epic (purple)
	Add(TEXT("Executioner"), TEXT("+45% weapon damage"), ECardEffect::WeaponDamageUp, 0.45f, EItemRarity::Epic);
	Add(TEXT("Hyper Loader"), TEXT("+50% fire rate"), ECardEffect::FireRateUp, 0.50f, EItemRarity::Epic);
	Add(TEXT("Charmed"), TEXT("Luck +3"), ECardEffect::LuckUp, 3.f, EItemRarity::Epic);
	// Legendary (yellow)
	Add(TEXT("Annihilator"), TEXT("+80% weapon damage"), ECardEffect::WeaponDamageUp, 0.80f, EItemRarity::Legendary);
	Add(TEXT("Juggernaut"), TEXT("+100 max health"), ECardEffect::MaxHealthUp, 100.f, EItemRarity::Legendary);
	Add(TEXT("Fortune's Favor"), TEXT("Luck +5"), ECardEffect::LuckUp, 5.f, EItemRarity::Legendary);
	return C;
}

EItemRarity UCardSystem::RollCardRarity(int32 RarityBias)
{
	const float B = static_cast<float>(FMath::Max(0, RarityBias));
	const float R = FMath::FRand();
	if (R < 0.05f + 0.04f * B) { return EItemRarity::Legendary; }
	if (R < 0.16f + 0.08f * B) { return EItemRarity::Epic; }
	if (R < 0.42f + 0.10f * B) { return EItemRarity::Rare; }
	return EItemRarity::Common;
}

TArray<FCardRow> UCardSystem::GetAllCards() const
{
	TArray<FCardRow> Out;
	if (CardTable)
	{
		TArray<FCardRow*> Rows;
		CardTable->GetAllRows<FCardRow>(TEXT("CardSystem"), Rows);
		for (const FCardRow* R : Rows)
		{
			if (R)
			{
				Out.Add(*R);
			}
		}
	}
	if (Out.Num() == 0)
	{
		Out = DefaultCards();
	}
	return Out;
}

TArray<FCardRow> UCardSystem::OfferCards(int32 Count, int32 RarityBias)
{
	EnsureTable();
	TArray<FCardRow> Pool = GetAllCards();
	TArray<FCardRow> Offer;

	for (int32 i = 0; i < Count && Pool.Num() > 0; ++i)
	{
		const EItemRarity Want = RollCardRarity(RarityBias);

		// Candidates of the wanted rarity; if none left, allow any.
		TArray<int32> Cand;
		for (int32 j = 0; j < Pool.Num(); ++j)
		{
			if (Pool[j].Rarity == Want)
			{
				Cand.Add(j);
			}
		}
		if (Cand.Num() == 0)
		{
			for (int32 j = 0; j < Pool.Num(); ++j)
			{
				Cand.Add(j);
			}
		}

		const int32 Idx = Cand[FMath::RandRange(0, Cand.Num() - 1)];
		Offer.Add(Pool[Idx]);
		Pool.RemoveAtSwap(Idx);
	}
	return Offer;
}
