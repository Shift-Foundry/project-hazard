#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
class UStaticMesh;
#include "HazardTypes.generated.h"

/** Firing behaviour of a weapon. */
UENUM(BlueprintType)
enum class EWeaponMode : uint8
{
	RangedHitscan	UMETA(DisplayName = "Ranged (Hitscan)"),
	RangedProjectile UMETA(DisplayName = "Ranged (Projectile)"),
	Melee			UMETA(DisplayName = "Melee")
};

/** Enemy archetypes (Phase 2 — data lives in C++; promoted to a DataTable in Phase 3). */
UENUM(BlueprintType)
enum class EZombieArchetype : uint8
{
	Walker	UMETA(DisplayName = "Walker"),
	Runner	UMETA(DisplayName = "Runner"),
	Brute	UMETA(DisplayName = "Brute")
};

/** Per-archetype tunables applied to AZombieBase on spawn (also a DT_Enemies row). */
USTRUCT(BlueprintType)
struct FZombieArchetypeStats : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	float MaxHealth = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	float MoveSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	float MeshScaleMul = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	int32 ScoreValue = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	int32 CoinValue = 1;
};

// ---------------------------------------------------------------- Phase 7: economy & loot

/** Item/card rarity tiers. Exclusive = chest-only, never sold in the shop. */
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	Common		UMETA(DisplayName = "Common"),
	Uncommon	UMETA(DisplayName = "Uncommon"),
	Rare		UMETA(DisplayName = "Rare"),
	Epic		UMETA(DisplayName = "Epic"),
	Legendary	UMETA(DisplayName = "Legendary"),
	Exclusive	UMETA(DisplayName = "Exclusive")
};

/** Optional weapon affixes (stack on top of base stats). */
UENUM(BlueprintType)
enum class EAffix : uint8
{
	None		UMETA(DisplayName = "None"),
	Crit		UMETA(DisplayName = "Crit"),          // chance for 2x damage
	Pierce		UMETA(DisplayName = "Pierce"),        // hitscan passes through
	Lifesteal	UMETA(DisplayName = "Lifesteal"),     // heals the Base on hit
	Burn		UMETA(DisplayName = "Burn")           // bonus damage over time (reserved)
};

/** DataTable row defining a weapon (DT_Weapons). Keyed by row name. */
USTRUCT(BlueprintType)
struct FWeaponDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") FName RowName;          // stable identity/unlock key (DT row name; falls back to DisplayName)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") EWeaponMode Mode = EWeaponMode::RangedHitscan;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") EItemRarity Rarity = EItemRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float Damage = 14.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float FireInterval = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") int32 MaxAmmo = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float ProjectileSpeed = 3500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float MeleeRange = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float MeleeRadius = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") TArray<EAffix> Affixes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") int32 CoinCost = 0;     // 0 / chest-only = not buyable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") bool bChestOnly = false; // Exclusive: only from chests
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") TSoftObjectPtr<UStaticMesh> Mesh;
};

/** DataTable row defining a player class (DT_Classes). Keyed by row name. */
USTRUCT(BlueprintType)
struct FClassDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") FName StartWeaponRow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 CoinCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") float DamageMult = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") float HealthBonus = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 WeaponSlots = 2; // class-upgradeable arsenal size
};

// ---------------------------------------------------------------- Phase 3: roguelite loop

/** Overall run flow state (mirrored to clients via GameState). */
UENUM(BlueprintType)
enum class ERunState : uint8
{
	NotStarted	UMETA(DisplayName = "Not Started"),
	Playing		UMETA(DisplayName = "Playing"),
	ChoosingCard UMETA(DisplayName = "Choosing Card"),
	GameOver	UMETA(DisplayName = "Game Over")
};

/** What an upgrade card does. Effects are stackable run modifiers. */
UENUM(BlueprintType)
enum class ECardEffect : uint8
{
	WeaponDamageUp		UMETA(DisplayName = "Weapon Damage +"),
	FireRateUp			UMETA(DisplayName = "Fire Rate +"),
	MeleeDamageUp		UMETA(DisplayName = "Melee Damage +"),
	MaxHealthUp			UMETA(DisplayName = "Max Health +"),
	AmmoCapacityUp		UMETA(DisplayName = "Ammo Capacity +"),
	ProjectileSpeedUp	UMETA(DisplayName = "Projectile Speed +"),
	LuckUp				UMETA(DisplayName = "Luck +")
};

/** DataTable row for upgrade cards (DT_Cards). Keyed by row name. */
USTRUCT(BlueprintType)
struct FCardRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	ECardEffect Effect = ECardEffect::WeaponDamageUp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	float Magnitude = 0.15f;

	/** Common(gray) / Rare(blue) / Epic(purple) / Legendary(yellow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	EItemRarity Rarity = EItemRarity::Common;
};

