#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DamageSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHazardActorKilled, AActor*, Victim, AActor*, Killer);

/**
 * Central, server-authoritative damage routing. Weapons/zombies call ApplyDamage here; it
 * validates authority, applies via the target's UHealthComponent, and on a kill increments
 * the run score (GameState) and broadcasts OnActorKilled.
 */
UCLASS()
class HAZARD_API UDamageSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server-only. Returns damage actually applied. */
	UFUNCTION(BlueprintCallable, Category = "Hazard")
	float ApplyDamage(AActor* Target, float Amount, AActor* Causer, const FVector& HitLocation);

	UFUNCTION(BlueprintPure, Category = "Hazard")
	int32 GetKillCount() const { return KillCount; }

	UPROPERTY(BlueprintAssignable, Category = "Hazard")
	FOnHazardActorKilled OnActorKilled;

private:
	int32 KillCount = 0;
};
