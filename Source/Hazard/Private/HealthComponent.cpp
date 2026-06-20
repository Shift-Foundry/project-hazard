#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CurrentHealth = MaxHealth;
		bDead = false;
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, CurrentHealth);
	DOREPLIFETIME(UHealthComponent, MaxHealth);
	DOREPLIFETIME(UHealthComponent, bDead);
}

void UHealthComponent::OnRep_MaxHealth()
{
	// Keep client-side bars / percentages correct when the server changes the cap.
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

float UHealthComponent::ApplyDamage(float Amount, AActor* DamageInstigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead || Amount <= 0.f)
	{
		return CurrentHealth;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnDamaged.Broadcast(Amount, DamageInstigator);

	if (CurrentHealth <= 0.f && !bDead)
	{
		bDead = true;
		HandleDeath(DamageInstigator);
	}
	return CurrentHealth;
}

void UHealthComponent::Heal(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead || Amount <= 0.f)
	{
		return;
	}
	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::ResetToFull()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	CurrentHealth = MaxHealth;
	bDead = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::OnRep_CurrentHealth()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::OnRep_Dead()
{
	// Mirror death to clients so client-side reactions (hide hands, SFX, HUD) run.
	if (bDead)
	{
		OnDeath.Broadcast(nullptr);
	}
}

void UHealthComponent::HandleDeath(AActor* Killer)
{
	OnDeath.Broadcast(Killer);
}
