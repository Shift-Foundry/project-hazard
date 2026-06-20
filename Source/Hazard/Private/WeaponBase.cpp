#include "WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "DamageSystem.h"
#include "HealthComponent.h"
#include "HazardProjectile.h"
#include "ProjectilePoolSubsystem.h"
#include "ZombieBase.h"
#include "RunManager.h"
#include "HazardFXSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMesh.h"

namespace
{
	URunManager* GetRunMgr(const UWorld* W)
	{
		return W ? W->GetSubsystem<URunManager>() : nullptr;
	}
}
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "CollisionShape.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	SetRootComponent(WeaponRoot);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(WeaponRoot);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.5f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		MeshComp->SetStaticMesh(Cube.Object);
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> FireCue(TEXT("/Game/XRFramework/Audio/Fire_Cue.Fire_Cue"));
	if (FireCue.Succeeded())
	{
		FireSound = FireCue.Object;
	}

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(MeshComp);
	Muzzle->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		CurrentAmmo = MaxAmmo;
	}
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeaponBase, CurrentAmmo);
	DOREPLIFETIME(AWeaponBase, RepMesh);
	DOREPLIFETIME(AWeaponBase, Rarity);
}

float AWeaponBase::RarityDamageMult(EItemRarity InRarity)
{
	switch (InRarity)
	{
	case EItemRarity::Uncommon:  return 1.15f;
	case EItemRarity::Rare:      return 1.35f;
	case EItemRarity::Epic:      return 1.6f;
	case EItemRarity::Legendary: return 2.0f;
	case EItemRarity::Exclusive: return 2.5f;
	default:                     return 1.0f;
	}
}

float AWeaponBase::ApplyCrit(float InDamage) const
{
	if (HasAffix(EAffix::Crit) && FMath::FRand() < 0.2f)
	{
		return InDamage * 2.f;
	}
	return InDamage;
}

void AWeaponBase::InitFromDef(const FWeaponDef& Def)
{
	Mode = Def.Mode;
	Rarity = Def.Rarity;
	DisplayName = Def.DisplayName;
	Affixes = Def.Affixes;
	Damage = Def.Damage * RarityDamageMult(Def.Rarity);
	FireInterval = Def.FireInterval;
	MaxAmmo = Def.MaxAmmo;
	ProjectileSpeed = Def.ProjectileSpeed;
	MeleeRange = Def.MeleeRange;
	MeleeRadius = Def.MeleeRadius;
	CurrentAmmo = MaxAmmo;
	bReloading = false;

	if (!Def.Mesh.IsNull())
	{
		if (UStaticMesh* M = Def.Mesh.LoadSynchronous())
		{
			RepMesh = M;
			if (MeshComp)
			{
				MeshComp->SetStaticMesh(M);
			}
		}
	}
}

void AWeaponBase::OnRep_WeaponVisual()
{
	if (RepMesh && MeshComp)
	{
		MeshComp->SetStaticMesh(RepMesh);
	}
}

void AWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || Mode == EWeaponMode::Melee)
	{
		return;
	}
	// Finish a reload when its timer elapses.
	if (bReloading && GetWorld()->GetTimeSeconds() >= ReloadEndTime)
	{
		int32 Bonus = 0;
		if (URunManager* RM = GetRunMgr(GetWorld()))
		{
			Bonus = RM->GetBonusAmmo();
		}
		CurrentAmmo = MaxAmmo + Bonus;
		bReloading = false;
	}
}

bool AWeaponBase::CanFire(float Now)
{
	if (Mode == EWeaponMode::Melee)
	{
		return (Now - LastFireTime) >= MeleeInterval;
	}

	if (bReloading)
	{
		return false;
	}
	if (CurrentAmmo <= 0)
	{
		bReloading = true;
		ReloadEndTime = Now + ReloadTime;
		return false;
	}
	float Interval = FireInterval;
	if (URunManager* RM = GetRunMgr(GetWorld()))
	{
		Interval = FireInterval / FMath::Max(RM->GetFireRateMult(), 0.01f);
	}
	return (Now - LastFireTime) >= Interval;
}

void AWeaponBase::TryFire(const FVector& AimOrigin, const FVector& AimDir, AActor* InstigatorActor)
{
	if (HasAuthority())
	{
		DoFireAuthoritative(AimOrigin, AimDir, InstigatorActor);
	}
	else
	{
		ServerFire(AimOrigin, AimDir, InstigatorActor);
	}
}

void AWeaponBase::ServerFire_Implementation(FVector Origin, FVector Dir, AActor* InstigatorActor)
{
	DoFireAuthoritative(Origin, Dir, InstigatorActor);
}

void AWeaponBase::DoFireAuthoritative(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!CanFire(Now))
	{
		return;
	}
	// Reject degenerate / spoofed aim from a client RPC before any state changes.
	if (Origin.ContainsNaN() || Dir.ContainsNaN() || Dir.IsNearlyZero())
	{
		return;
	}
	LastFireTime = Now;

	if (Mode == EWeaponMode::Melee)
	{
		DoMelee(Origin, Dir, InstigatorActor); // DoMelee already broadcasts MulticastMeleeFX
		return;
	}

	--CurrentAmmo;
	if (Mode == EWeaponMode::RangedProjectile)
	{
		FireProjectile(Origin, Dir, InstigatorActor);
	}
	else
	{
		FireHitscan(Origin, Dir, InstigatorActor);
	}

	if (CurrentAmmo <= 0)
	{
		bReloading = true;
		ReloadEndTime = Now + ReloadTime;
	}
}

void AWeaponBase::FireHitscan(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor)
{
	const FVector End = Origin + Dir.GetSafeNormal() * Range;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HazardHitscan), false);
	Params.AddIgnoredActor(this);
	if (InstigatorActor)
	{
		Params.AddIgnoredActor(InstigatorActor);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params);
	const FVector ImpactPoint = bHit ? Hit.ImpactPoint : End;

	// Only enemies take weapon damage (no friendly fire on the Base or other players).
	if (bHit && Cast<AZombieBase>(Hit.GetActor()))
	{
		if (UDamageSystem* DS = GetWorld()->GetSubsystem<UDamageSystem>())
		{
			float Dmg = Damage;
			if (URunManager* RM = GetRunMgr(GetWorld()))
			{
				Dmg *= RM->GetDamageMult();
			}
			Dmg = ApplyCrit(Dmg);
			DS->ApplyDamage(Hit.GetActor(), Dmg, InstigatorActor, Hit.ImpactPoint);
		}
	}

	MulticastRangedFX(Origin, ImpactPoint);
}

void AWeaponBase::FireProjectile(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor)
{
	if (UProjectilePoolSubsystem* Pool = GetWorld()->GetSubsystem<UProjectilePoolSubsystem>())
	{
		TSubclassOf<AHazardProjectile> Cls = ProjectileClass;
		if (!Cls)
		{
			Cls = AHazardProjectile::StaticClass();
		}
		if (AHazardProjectile* Proj = Pool->Acquire(Cls))
		{
			const FVector Start = Muzzle ? Muzzle->GetComponentLocation() : Origin;
			float Speed = ProjectileSpeed;
			float Dmg = Damage;
			if (URunManager* RM = GetRunMgr(GetWorld()))
			{
				Speed *= RM->GetProjectileSpeedMult();
				Dmg *= RM->GetDamageMult();
			}
			Dmg = ApplyCrit(Dmg);
			Proj->Launch(Start, Dir, Speed, Dmg, 2.5f, InstigatorActor);
		}
	}
	MulticastRangedFX(Origin, Origin + Dir.GetSafeNormal() * 120.f);
}

void AWeaponBase::DoMelee(const FVector& Origin, const FVector& Dir, AActor* InstigatorActor)
{
	const FVector Center = Origin + Dir.GetSafeNormal() * (MeleeRange * 0.5f);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HazardMelee), false);
	Params.AddIgnoredActor(this);
	if (InstigatorActor)
	{
		Params.AddIgnoredActor(InstigatorActor);
	}

	GetWorld()->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(MeleeRadius), Params);

	TSet<AActor*> Damaged;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || Damaged.Contains(A) || !Cast<AZombieBase>(A))
		{
			continue;
		}
		Damaged.Add(A);
		if (UDamageSystem* DS = GetWorld()->GetSubsystem<UDamageSystem>())
		{
			float Dmg = Damage;
			if (URunManager* RM = GetRunMgr(GetWorld()))
			{
				Dmg *= RM->GetMeleeDamageMult();
			}
			Dmg = ApplyCrit(Dmg);
			DS->ApplyDamage(A, Dmg, InstigatorActor, A->GetActorLocation());
		}
	}

	MulticastMeleeFX(Origin, Dir);
}

void AWeaponBase::MulticastRangedFX_Implementation(FVector Start, FVector End)
{
	if (UHazardFXSubsystem* FX = GetWorld() ? GetWorld()->GetSubsystem<UHazardFXSubsystem>() : nullptr)
	{
		FX->SpawnFlash(Start, FLinearColor(1.f, 0.9f, 0.25f), 14.f); // muzzle
		FX->SpawnFlash(End, FLinearColor(1.f, 0.5f, 0.12f), 20.f);   // impact
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Start);
	}
#if ENABLE_DRAW_DEBUG
	DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 0.12f, 0, 1.5f);
#endif
}

void AWeaponBase::MulticastMeleeFX_Implementation(FVector Origin, FVector Dir)
{
	const FVector Center = Origin + Dir.GetSafeNormal() * (MeleeRange * 0.5f);
	if (UHazardFXSubsystem* FX = GetWorld() ? GetWorld()->GetSubsystem<UHazardFXSubsystem>() : nullptr)
	{
		FX->SpawnFlash(Center, FLinearColor(0.2f, 0.9f, 1.f), MeleeRadius);
	}
#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), Center, MeleeRadius, 10, FColor::Cyan, false, 0.15f, 0, 1.5f);
#endif
}
