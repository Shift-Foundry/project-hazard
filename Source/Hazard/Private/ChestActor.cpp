#include "ChestActor.h"
#include "Components/StaticMeshComponent.h"
#include "RunManager.h"
#include "HazardFXSubsystem.h"
#include "WeaponLibrary.h"
#include "WeaponComponent.h"
#include "HazardPlayerPawn.h"
#include "HazardGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AChestActor::AChestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(SceneRoot);
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		MeshComp->SetStaticMesh(Cube.Object);
	}
	BaseScale = FVector(0.6f, 0.6f, 0.45f);
	MeshComp->SetRelativeScale3D(BaseScale);
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
}

void AChestActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(OpenHandle, this, &AChestActor::Open, FMath::Max(OpenDelay, 0.1f), false);
	}
}

void AChestActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AChestActor, bOpened);
}

void AChestActor::Open()
{
	if (!HasAuthority() || bOpened)
	{
		return;
	}
	bOpened = true;

	// Case-roll: weighted by milestone tier (+ run Luck) -> a rarity weapon, or a coin payout.
	URunManager* RM = GetWorld()->GetSubsystem<URunManager>();
	const int32 Luck = RM ? RM->GetLuck() : 0;
	const EItemRarity Rarity = UWeaponLibrary::RollRarityForTier(RarityTier, Luck);
	if (FMath::FRand() < 0.30f)
	{
		const int32 CoinReward = 60 * RarityTier + FMath::RandRange(0, 60);
		if (AHazardGameState* GS = GetWorld()->GetGameState<AHazardGameState>())
		{
			GS->AddCoins(CoinReward);
		}
		UE_LOG(LogTemp, Display, TEXT("[Chest] tier %d -> %d coins"), RarityTier, CoinReward);
	}
	else
	{
		const FWeaponDef Def = RM ? RM->RollUnlockedWeapon(Rarity, /*bAllowChestOnly=*/true) : UWeaponLibrary::RollWeaponDef(Rarity);
		AHazardPlayerPawn* Nearest = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<AHazardPlayerPawn> It(GetWorld()); It; ++It)
		{
			const float D = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
			if (D < Best) { Best = D; Nearest = *It; }
		}
		if (Nearest)
		{
			Nearest->EquipToArsenal(Def); // route through the slot system (HUD + replicated mirror)
		}
		UE_LOG(LogTemp, Display, TEXT("[Chest] tier %d -> weapon %s [%s]"), RarityTier, *Def.DisplayName, *UWeaponLibrary::RarityName(Def.Rarity));
	}

	MulticastOpenFX();
	SetLifeSpan(6.f);
}

void AChestActor::MulticastOpenFX_Implementation()
{
	bOpened = true;
	if (MeshComp)
	{
		// "Pop open": flatten the lid.
		MeshComp->SetRelativeScale3D(FVector(BaseScale.X * 1.2f, BaseScale.Y * 1.2f, BaseScale.Z * 0.3f));
	}
	if (UHazardFXSubsystem* FX = GetWorld() ? GetWorld()->GetSubsystem<UHazardFXSubsystem>() : nullptr)
	{
		FX->SpawnFlash(GetActorLocation() + FVector(0.f, 0.f, 40.f), FLinearColor(1.f, 0.8f, 0.2f), 45.f);
	}
}

void AChestActor::OnRep_Opened()
{
	if (bOpened && MeshComp)
	{
		MeshComp->SetRelativeScale3D(FVector(BaseScale.X * 1.2f, BaseScale.Y * 1.2f, BaseScale.Z * 0.3f));
	}
}
