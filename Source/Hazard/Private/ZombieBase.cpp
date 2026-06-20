#include "ZombieBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HealthComponent.h"
#include "HazardBase.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "HazardFXSubsystem.h"
#include "HazardGameMode.h"
#include "HazardGameState.h"
#include "ChestActor.h"
#include "WeaponLibrary.h"
#include "RunManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AZombieBase::AZombieBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	SetRootComponent(Capsule);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(Capsule);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ZombieMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ZombieMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(ZombieMesh.Object);
	}
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	MeshComp->SetRelativeScale3D(DesignBaseScale);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 30.f;
}

void AZombieBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AZombieBase, Archetype);
	DOREPLIFETIME(AZombieBase, bIsBoss);
}

FZombieArchetypeStats AZombieBase::GetArchetypeStats(EZombieArchetype InArchetype)
{
	// Prefer DT_Enemies (designer data) keyed by archetype name; fall back to C++ defaults.
	FName RowName = NAME_None;
	switch (InArchetype)
	{
	case EZombieArchetype::Runner: RowName = TEXT("Runner"); break;
	case EZombieArchetype::Brute:  RowName = TEXT("Brute"); break;
	case EZombieArchetype::Walker: default: RowName = TEXT("Walker"); break;
	}
	if (UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Hazard/Data/DT_Enemies.DT_Enemies")))
	{
		if (const FZombieArchetypeStats* Row = DT->FindRow<FZombieArchetypeStats>(RowName, TEXT("GetArchetypeStats"), false))
		{
			return *Row;
		}
	}

	FZombieArchetypeStats S;
	switch (InArchetype)
	{
	case EZombieArchetype::Runner:
		S.MaxHealth = 18.f;  S.MoveSpeed = 280.f; S.AttackDamage = 6.f;  S.MeshScaleMul = 0.8f; S.ScoreValue = 15; S.CoinValue = 2;
		break;
	case EZombieArchetype::Brute:
		S.MaxHealth = 110.f; S.MoveSpeed = 70.f;  S.AttackDamage = 28.f; S.MeshScaleMul = 1.5f; S.ScoreValue = 30; S.CoinValue = 5;
		break;
	case EZombieArchetype::Walker:
	default:
		S.MaxHealth = 30.f;  S.MoveSpeed = 120.f; S.AttackDamage = 10.f; S.MeshScaleMul = 1.0f; S.ScoreValue = 10; S.CoinValue = 1;
		break;
	}
	return S;
}

void AZombieBase::BeginPlay()
{
	Super::BeginPlay();

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &AZombieBase::HandleDeath);
		Health->OnDamaged.AddDynamic(this, &AZombieBase::HandleDamaged);
	}
	ApplyArchetypeVisuals();
}

void AZombieBase::SetArchetype(EZombieArchetype InArchetype)
{
	if (!HasAuthority())
	{
		return;
	}
	Archetype = InArchetype;
	const FZombieArchetypeStats S = GetArchetypeStats(Archetype);
	MoveSpeed = S.MoveSpeed;
	AttackDamage = S.AttackDamage;
	ScoreValue = S.ScoreValue;
	CoinValue = S.CoinValue;
	if (Health)
	{
		Health->MaxHealth = S.MaxHealth;
		Health->ResetToFull();
	}
	ApplyArchetypeVisuals();
}

void AZombieBase::OnRep_Archetype()
{
	ApplyArchetypeVisuals();
}

void AZombieBase::SetBoss(int32 Tier)
{
	if (!HasAuthority())
	{
		return;
	}
	bIsBoss = true;
	BossTier = FMath::Max(1, Tier);
	if (Health)
	{
		Health->MaxHealth = 300.f * (1.f + BossTier * 0.5f);
		Health->ResetToFull();
	}
	MoveSpeed = 90.f;
	AttackDamage = 40.f;
	AttackRange = 200.f;
	ScoreValue = 200 * BossTier;
	CoinValue = 50 * BossTier;
	ApplyArchetypeVisuals(); // boss-aware; clients re-apply via OnRep_Boss
}

void AZombieBase::OnRep_Boss()
{
	ApplyArchetypeVisuals(); // clients enlarge the boss once bIsBoss replicates
}

void AZombieBase::ApplyArchetypeVisuals()
{
	const FZombieArchetypeStats S = GetArchetypeStats(Archetype);
	CurrentBaseScale = bIsBoss ? (DesignBaseScale * 3.f) : (DesignBaseScale * S.MeshScaleMul);
	if (MeshComp && HitReactRemaining <= 0.f)
	{
		MeshComp->SetRelativeScale3D(CurrentBaseScale);
	}
	if (bIsBoss && Capsule)
	{
		Capsule->SetCapsuleSize(80.f, 160.f);
	}
}

void AZombieBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHitReact(DeltaSeconds);

	if (!HasAuthority() || State == EZombieState::Dead)
	{
		return;
	}

	AActor* Target = ResolveTarget();
	if (!Target)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	FVector TargetLoc = Target->GetActorLocation();
	TargetLoc.Z = MyLoc.Z;

	const FVector ToTarget = TargetLoc - MyLoc;
	const float Dist = ToTarget.Size();

	if (Dist > AttackRange)
	{
		State = EZombieState::Seeking;
		const FVector Dir = ToTarget.GetSafeNormal();
		AddActorWorldOffset(Dir * MoveSpeed * DeltaSeconds, /*bSweep=*/false);
		if (!Dir.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		}
	}
	else
	{
		State = EZombieState::Attacking;
		TimeSinceAttack += DeltaSeconds;
		if (TimeSinceAttack >= AttackInterval)
		{
			TimeSinceAttack = 0.f;
			if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
			{
				TargetHealth->ApplyDamage(AttackDamage, this);
			}
		}
	}
}

AActor* AZombieBase::ResolveTarget()
{
	if (TargetActor)
	{
		return TargetActor;
	}
	for (TActorIterator<AHazardBase> It(GetWorld()); It; ++It)
	{
		TargetActor = *It;
		break;
	}
	return TargetActor;
}

void AZombieBase::HandleDamaged(float Damage, AActor* Causer)
{
	MulticastHitReact();
}

void AZombieBase::MulticastHitReact_Implementation()
{
	HitReactRemaining = 0.12f;
	if (MeshComp)
	{
		MeshComp->SetRelativeScale3D(CurrentBaseScale * 1.25f);
	}
}

void AZombieBase::UpdateHitReact(float DeltaSeconds)
{
	if (HitReactRemaining > 0.f)
	{
		HitReactRemaining -= DeltaSeconds;
		if (HitReactRemaining <= 0.f && MeshComp && State != EZombieState::Dead)
		{
			MeshComp->SetRelativeScale3D(CurrentBaseScale);
		}
	}
}

void AZombieBase::HandleDeath(AActor* Killer)
{
	State = EZombieState::Dead;
	if (Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (bIsBoss && HasAuthority())
	{
		if (AHazardGameMode* GM = GetWorld()->GetAuthGameMode<AHazardGameMode>())
		{
			TSubclassOf<AChestActor> Cls = GM->ChestClass;
			if (!Cls)
			{
				Cls = AChestActor::StaticClass();
			}
			FVector Loc = GetActorLocation();
			Loc.Z = 0.f;
			if (AChestActor* Chest = GetWorld()->SpawnActor<AChestActor>(Cls, FTransform(FRotator::ZeroRotator, Loc)))
			{
				Chest->RarityTier = FMath::Clamp(BossTier, 1, 5);
				Chest->OpenDelay = 3.f;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[Boss] Boss (tier %d) defeated -> milestone chest"), BossTier);
	}
	// Note: normal-kill weapon drops were replaced by the in-run HUD RNG roller (URunManager).

	MulticastDeathFX();
	SetLifeSpan(1.5f);
}

void AZombieBase::MulticastDeathFX_Implementation()
{
	State = EZombieState::Dead;
	if (MeshComp)
	{
		// Collapse: flatten the silhouette.
		MeshComp->SetRelativeScale3D(FVector(CurrentBaseScale.X * 1.3f, CurrentBaseScale.Y * 1.3f, 0.2f));
	}
	if (UHazardFXSubsystem* FX = GetWorld() ? GetWorld()->GetSubsystem<UHazardFXSubsystem>() : nullptr)
	{
		FX->SpawnFlash(GetActorLocation() + FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.15f, 0.15f), 55.f);
	}
}
