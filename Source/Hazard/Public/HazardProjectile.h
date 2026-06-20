#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HazardProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Lightweight pooled projectile. Server-driven straight-line motion + overlap damage; returns
 * itself to UProjectilePoolSubsystem on hit or expiry instead of being destroyed.
 */
UCLASS()
class HAZARD_API AHazardProjectile : public AActor
{
	GENERATED_BODY()

public:
	AHazardProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/** Server: activate from the pool and send it flying. */
	void Launch(const FVector& StartLocation, const FVector& Direction, float Speed, float InDamage, float InLifeSeconds, AActor* InInstigator);

	/** Return to the pool (hidden, no tick/collision). */
	void Deactivate();

	bool IsActiveProjectile() const { return bActiveProjectile; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UStaticMeshComponent* MeshComp;

protected:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ReturnToPool();

private:
	bool bActiveProjectile = false;
	float Damage = 10.f;
	float LifeRemaining = 0.f;
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AActor> ShooterActor = nullptr;
};
