#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HazardTypes.h"
#include "ZombieBase.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UHealthComponent;

UENUM(BlueprintType)
enum class EZombieState : uint8
{
	Seeking		UMETA(DisplayName = "Seeking"),
	Attacking	UMETA(DisplayName = "Attacking"),
	Dead		UMETA(DisplayName = "Dead")
};

/**
 * Server-authoritative enemy with data-driven archetypes (Walker/Runner/Brute), a simple
 * Seeking/Attacking AI state, replicated movement, and hit-react / death feedback.
 */
UCLASS()
class HAZARD_API AZombieBase : public APawn
{
	GENERATED_BODY()

public:
	AZombieBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie")
	float MoveSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	float AttackRange = 140.f;

	UPROPERTY(BlueprintReadOnly, Category = "Zombie")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie")
	float AttackInterval = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie")
	UCapsuleComponent* Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie")
	UHealthComponent* Health;

	void SetTargetActor(AActor* InTarget) { TargetActor = InTarget; }

	/** Server: apply an archetype's stats and reset health to the new max. */
	void SetArchetype(EZombieArchetype InArchetype);

	/** Server: promote to a boss (scales stats/size; drops a milestone chest on death). */
	void SetBoss(int32 Tier);

	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsBoss() const { return bIsBoss; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	int32 GetScoreValue() const { return ScoreValue; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	int32 GetCoinValue() const { return CoinValue; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	EZombieArchetype GetArchetype() const { return Archetype; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	EZombieState GetState() const { return State; }

	static FZombieArchetypeStats GetArchetypeStats(EZombieArchetype InArchetype);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Archetype)
	EZombieArchetype Archetype = EZombieArchetype::Walker;

	UFUNCTION()
	void OnRep_Archetype();

	UFUNCTION()
	void OnRep_Boss();

	UPROPERTY()
	TObjectPtr<AActor> TargetActor = nullptr;

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	UFUNCTION()
	void HandleDamaged(float Damage, AActor* Causer);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHitReact();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDeathFX();

	AActor* ResolveTarget();
	void ApplyArchetypeVisuals();
	void UpdateHitReact(float DeltaSeconds);

private:
	EZombieState State = EZombieState::Seeking;
	int32 ScoreValue = 10;
	int32 CoinValue = 1;
	UPROPERTY(ReplicatedUsing = OnRep_Boss)
	bool bIsBoss = false;
	int32 BossTier = 1;
	float TimeSinceAttack = 0.f;
	float HitReactRemaining = 0.f;

	const FVector DesignBaseScale = FVector(0.6f, 0.6f, 1.7f);
	FVector CurrentBaseScale = FVector(0.6f, 0.6f, 1.7f);
};
