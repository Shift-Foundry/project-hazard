#include "HazardFXSubsystem.h"
#include "HazardFlashFX.h"
#include "ShiftXRPerfGovernor.h"
#include "Engine/World.h"

void UHazardFXSubsystem::SpawnFlash(const FVector& Location, const FLinearColor& Color, float Size)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	// Honor the performance governor: scale down / skip cosmetic FX on lower tiers.
	float VFXScale = 1.f;
	if (const UShiftXRPerfGovernor* PG = W->GetSubsystem<UShiftXRPerfGovernor>())
	{
		VFXScale = PG->GetVFXScale();
	}
	if (VFXScale <= 0.f)
	{
		return;
	}

	AHazardFlashFX* Flash = nullptr;
	for (const TObjectPtr<AHazardFlashFX>& P : Pool)
	{
		if (IsValid(P) && !P->IsActiveFX())
		{
			Flash = P;
			break;
		}
	}

	if (!Flash)
	{
		if (Pool.Num() >= MaxPool)
		{
			return; // hard cap — never unbounded
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Flash = W->SpawnActor<AHazardFlashFX>(AHazardFlashFX::StaticClass(), FTransform::Identity, Params);
		if (Flash)
		{
			Pool.Add(Flash);
		}
	}

	if (Flash)
	{
		Flash->Play(Location, Color, Size * FMath::Lerp(0.7f, 1.f, VFXScale), 0.15f);
	}
}
