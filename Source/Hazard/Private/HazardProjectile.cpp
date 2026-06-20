#include "HazardProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DamageSystem.h"
#include "HealthComponent.h"
#include "ProjectilePoolSubsystem.h"
#include "ZombieBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AHazardProjectile::AHazardProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(Collision);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(Collision);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(0.22f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		MeshComp->SetStaticMesh(Sphere.Object);
	}

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AHazardProjectile::OnOverlap);
}

void AHazardProjectile::Launch(const FVector& StartLocation, const FVector& Direction, float Speed, float InDamage, float InLifeSeconds, AActor* InInstigator)
{
	Damage = InDamage;
	LifeRemaining = InLifeSeconds;
	Velocity = Direction.GetSafeNormal() * Speed;
	ShooterActor = InInstigator;

	SetActorLocation(StartLocation);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	bActiveProjectile = true;
}

void AHazardProjectile::Deactivate()
{
	bActiveProjectile = false;
	LifeRemaining = 0.f;
	Velocity = FVector::ZeroVector;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void AHazardProjectile::ReturnToPool()
{
	if (UWorld* W = GetWorld())
	{
		if (UProjectilePoolSubsystem* Pool = W->GetSubsystem<UProjectilePoolSubsystem>())
		{
			Pool->Release(this);
			return;
		}
	}
	Deactivate();
}

void AHazardProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActiveProjectile || !HasAuthority())
	{
		return;
	}

	AddActorWorldOffset(Velocity * DeltaSeconds, /*bSweep=*/false);

	LifeRemaining -= DeltaSeconds;
	if (LifeRemaining <= 0.f)
	{
		ReturnToPool();
	}
}

void AHazardProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bActiveProjectile || !HasAuthority())
	{
		return;
	}
	if (!OtherActor || OtherActor == this || OtherActor == ShooterActor || !Cast<AZombieBase>(OtherActor))
	{
		return;
	}

	if (UHealthComponent* HC = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		if (!HC->IsDead())
		{
			if (UDamageSystem* DS = GetWorld()->GetSubsystem<UDamageSystem>())
			{
				DS->ApplyDamage(OtherActor, Damage, ShooterActor, GetActorLocation());
			}
			ReturnToPool();
		}
	}
}
