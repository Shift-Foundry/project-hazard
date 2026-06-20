#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "ProjectilePoolSubsystem.generated.h"

class AHazardProjectile;

/** Server-side reuse pool for AHazardProjectile so rapid fire doesn't spawn/destroy churn. */
UCLASS()
class HAZARD_API UProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Get a ready (inactive) projectile of the given class, spawning one if the pool is empty. */
	AHazardProjectile* Acquire(TSubclassOf<AHazardProjectile> ProjClass);

	/** Deactivate and return a projectile to the pool. */
	void Release(AHazardProjectile* Proj);

private:
	UPROPERTY()
	TArray<TObjectPtr<AHazardProjectile>> Inactive;
};
