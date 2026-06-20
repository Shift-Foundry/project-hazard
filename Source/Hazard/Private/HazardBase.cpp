#include "HazardBase.h"
#include "Components/StaticMeshComponent.h"
#include "HealthComponent.h"
#include "ShiftXRSceneAnchor.h"
#include "ShiftXRTypes.h"
#include "RunManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AHazardBase::AHazardBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SceneRoot);
	// Greybox fallback: a chunky cube sitting on the floor. BP_HazardBase swaps this for
	// SM_GB_Base + M_Greybox via Python authoring.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BaseMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(BaseMesh.Object);
	}
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	MeshComp->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.0f));
	// The Base must not block weapon traces (the player would otherwise shoot its own
	// objective when a zombie is on the far side). It is damaged only by zombie melee.
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	SceneAnchor = CreateDefaultSubobject<UShiftXRSceneAnchor>(TEXT("SceneAnchor"));
	SceneAnchor->SetupAttachment(SceneRoot);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 1000.f;
}

void AHazardBase::BeginPlay()
{
	Super::BeginPlay();

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &AHazardBase::HandleBaseDestroyed);
	}

	if (HasAuthority() && SceneAnchor)
	{
		// Stub backend: drops the Base onto the surface below (real Android XR anchoring
		// swaps in behind WITH_ANDROIDXR when a 5.8-compatible plugin exists).
		FShiftAnchorHandle Handle;
		SceneAnchor->AnchorToSurfaceBelow(AnchorTraceDistance, Handle);
	}
}

void AHazardBase::HandleBaseDestroyed(AActor* Killer)
{
	UE_LOG(LogTemp, Warning, TEXT("[Hazard] Base destroyed — objective lost."));
	if (UWorld* W = GetWorld())
	{
		if (URunManager* RM = W->GetSubsystem<URunManager>())
		{
			RM->EndRun();
		}
	}
}
