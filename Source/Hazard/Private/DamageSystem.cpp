#include "DamageSystem.h"
#include "HealthComponent.h"
#include "HazardGameState.h"
#include "ZombieBase.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

float UDamageSystem::ApplyDamage(AActor* Target, float Amount, AActor* Causer, const FVector& HitLocation)
{
	if (!Target || Amount <= 0.f)
	{
		return 0.f;
	}

	UWorld* World = GetWorld();
	if (!World || !Target->HasAuthority())
	{
		return 0.f; // server-authoritative
	}

	UHealthComponent* HC = Target->FindComponentByClass<UHealthComponent>();
	if (!HC || HC->IsDead())
	{
		return 0.f;
	}

	const float Before = HC->GetHealth();
	HC->ApplyDamage(Amount, Causer);
	const float Applied = Before - HC->GetHealth();

	if (HC->IsDead())
	{
		++KillCount;

		int32 Points = 10;
		int32 Coins = 1;
		if (const AZombieBase* Zombie = Cast<AZombieBase>(Target))
		{
			Points = Zombie->GetScoreValue();
			Coins = Zombie->GetCoinValue();
		}
		if (AHazardGameState* GS = World->GetGameState<AHazardGameState>())
		{
			GS->AddScore(Points);
			GS->AddCoins(Coins);
		}
		OnActorKilled.Broadcast(Target, Causer);
		UE_LOG(LogTemp, Display, TEXT("[HazardDmg] KILL %s (+%d score, total kills %d)"), *Target->GetName(), Points, KillCount);
	}

	return Applied;
}
