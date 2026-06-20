#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "HazardTypes.h"
#include "WeaponBase.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class AHazardProjectile;

/**
 * A weapon with two modes — ranged (hitscan or pooled projectile) and melee (overlap arc).
 * Server-authoritative: TryFire routes to the server, which validates cooldown/ammo, applies
 * damage via UDamageSystem, and multicasts lightweight FX. Spawned & owned by UWeaponComponent.
 */
UCLASS()
class HAZARD_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EWeaponMode Mode = EWeaponMode::RangedHitscan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Damage = 12.f;

	/** Played on each shot/swing (defaults to the XR template fire cue; swap per weapon in BP). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ranged")
	float Range = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ranged")
	float FireInterval = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ranged")
	int32 MaxAmmo = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ranged")
	float ReloadTime = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ranged")
	float ProjectileSpeed = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ranged")
	TSubclassOf<AHazardProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee")
	float MeleeInterval = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee")
	float MeleeRange = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee")
	float MeleeRadius = 90.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USceneComponent* WeaponRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USceneComponent* Muzzle;

	/** Server: configure this weapon from a data-table definition (stats, rarity, affixes, mesh). */
	void InitFromDef(const FWeaponDef& Def);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	EItemRarity GetRarity() const { return Rarity; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FString GetDisplayName() const { return DisplayName; }

	static float RarityDamageMult(EItemRarity InRarity);

	/** Called on the owning client; handles authority + cooldown + ammo internally. */
	void TryFire(const FVector& AimOrigin, const FVector& AimDir, AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsRanged() const { return Mode != EWeaponMode::Melee; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bReloading; }

protected:
	UFUNCTION(Server, Reliable)
	void ServerFire(FVector Origin, FVector Dir, AActor* InstigatorActor);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRangedFX(FVector Start, FVector End);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeFX(FVector Origin, FVector Dir);

	void DoFireAuthoritative(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor);
	void FireHitscan(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor);
	void FireProjectile(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor);
	void DoMelee(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor);
	bool CanFire(float Now);

	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponVisual)
	TObjectPtr<UStaticMesh> RepMesh = nullptr;

	UPROPERTY(Replicated)
	EItemRarity Rarity = EItemRarity::Common;

	UFUNCTION()
	void OnRep_WeaponVisual();

	bool HasAffix(EAffix Affix) const { return Affixes.Contains(Affix); }
	float ApplyCrit(float InDamage) const;

private:
	float LastFireTime = -1000.f;
	float ReloadEndTime = -1.f;
	bool bReloading = false;

	TArray<EAffix> Affixes;
	FString DisplayName;
};
