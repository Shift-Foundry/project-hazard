#include "HazardFlashFX.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AHazardFlashFX::AHazardFlashFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMat(TEXT("/Game/Hazard/Materials/M_FlashFX.M_FlashFX"));
	if (FlashMat.Succeeded())
	{
		Mesh->SetMaterial(0, FlashMat.Object);
	}
}

void AHazardFlashFX::Play(const FVector& Location, const FLinearColor& Color, float Size, float InDuration)
{
	if (!MID)
	{
		MID = Mesh->CreateDynamicMaterialInstance(0);
	}
	BaseColor = Color;
	MaxSize = FMath::Max(Size, 1.f);
	Duration = FMath::Max(InDuration, 0.02f);
	Elapsed = 0.f;
	bActive = true;

	SetActorLocation(Location);
	const float StartScale = (MaxSize * 0.3f) / 50.f; // sphere mesh is 50cm radius
	SetActorScale3D(FVector(StartScale));
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);

	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), BaseColor);
	}
}

void AHazardFlashFX::Deactivate()
{
	bActive = false;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}

void AHazardFlashFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}

	Elapsed += DeltaSeconds;
	const float T = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	if (T >= 1.f)
	{
		Deactivate();
		return;
	}

	const float CurSize = FMath::Lerp(0.3f, 1.f, T) * MaxSize;
	SetActorScale3D(FVector(CurSize / 50.f));
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), BaseColor * (1.f - T)); // fade emissive to black
	}
}
