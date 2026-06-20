#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHazardOnHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHazardOnDeath, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHazardOnDamaged, float, Damage, AActor*, Causer);

/** Replicated, server-authoritative health. Damage is only applied on the server. */
UCLASS(ClassGroup = (Hazard), meta = (BlueprintSpawnableComponent))
class HAZARD_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Server-only. Returns remaining health. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float Amount, AActor* DamageInstigator);

	/** Server-only. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Server-only. Reset to full health and clear the dead flag (used when applying archetypes). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetToFull();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FHazardOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FHazardOnDeath OnDeath;

	/** Fired on the server whenever damage is applied (before any death handling). */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FHazardOnDamaged OnDamaged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth)
	float CurrentHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_MaxHealth();

	UFUNCTION()
	void OnRep_Dead();

private:
	void HandleDeath(AActor* Killer);
};
