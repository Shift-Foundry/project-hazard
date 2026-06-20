#include "ProjectilePoolSubsystem.h"
#include "HazardProjectile.h"
#include "Engine/World.h"

AHazardProjectile* UProjectilePoolSubsystem::Acquire(TSubclassOf<AHazardProjectile> ProjClass)
{
	if (!ProjClass)
	{
		return nullptr;
	}

	// Reuse an inactive projectile of the SAME class (never hand back a wrong subclass).
	for (int32 i = Inactive.Num() - 1; i >= 0; --i)
	{
		AHazardProjectile* Proj = Inactive[i];
		if (!IsValid(Proj))
		{
			Inactive.RemoveAtSwap(i);
			continue;
		}
		if (Proj->GetClass() == ProjClass)
		{
			Inactive.RemoveAtSwap(i);
			return Proj;
		}
	}

	// Otherwise spawn a fresh one (starts deactivated).
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHazardProjectile* NewProj = World->SpawnActor<AHazardProjectile>(ProjClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (NewProj)
	{
		NewProj->Deactivate();
	}
	return NewProj;
}

void UProjectilePoolSubsystem::Release(AHazardProjectile* Proj)
{
	if (!IsValid(Proj))
	{
		return;
	}
	Proj->Deactivate();
	Inactive.Add(Proj);
}
