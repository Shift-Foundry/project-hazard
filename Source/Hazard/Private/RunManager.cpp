#include "RunManager.h"
#include "WaveDirector.h"
#include "HazardGameMode.h"
#include "HazardGameState.h"
#include "CardSystem.h"
#include "HazardSaveGame.h"
#include "HazardPlayerPawn.h"
#include "HealthComponent.h"
#include "HazardBase.h"
#include "ChestActor.h"
#include "HazardGameInstance.h"
#include "ClassLibrary.h"
#include "WeaponLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarAutoPickCards(
	TEXT("hazard.AutoPickCards"), 0,
	TEXT("Debug: auto-pick the first offered card to progress levels headlessly."));

static TAutoConsoleVariable<int32> CVarAutoChest(
	TEXT("hazard.AutoChest"), 0,
	TEXT("Debug: spawn a chest near the Base at the start of each level."));

void URunManager::StartRun()
{
	LoadRecords();
	DamageMult = FireRateMult = MeleeDamageMult = ProjectileSpeedMult = 1.f;
	BonusMaxHealth = 0.f;
	ClassHealthBonus = 0.f;
	BonusAmmo = 0;
	Luck = 0;
	CoinsBankedThisRun = 0;
	CurrentLevel = 0;

	// Fold the selected class in (damage + health base), and load the unlocked-weapon pool for the run.
	if (UHazardGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UHazardGameInstance>() : nullptr)
	{
		const FClassDef Cls = UClassLibrary::FindClass(GI->GetSelectedClass());
		DamageMult *= Cls.DamageMult;
		ClassHealthBonus = Cls.HealthBonus;
		UnlockedWeaponRows = GI->GetUnlockedWeapons();
	}

	if (UWaveDirector* WD = GetWaveDirector())
	{
		if (!bBoundWaves)
		{
			WD->OnAllWavesCleared.AddDynamic(this, &URunManager::HandleAllWavesCleared);
			bBoundWaves = true;
		}
	}

	EstablishSharedAnchor();
	BeginNextLevel();

	// In-run weapon RNG roller: rerolls every 10s; only produces an offer while Playing.
	GetWorld()->GetTimerManager().SetTimer(RollerHandle, this, &URunManager::RollWeaponOffer, 10.f, true, 2.0f);
}

void URunManager::EstablishSharedAnchor()
{
	AHazardGameState* GS = GetGS();
	if (!GS)
	{
		return;
	}
	FTransform Anchor = FTransform::Identity;
	for (TActorIterator<AHazardBase> It(GetWorld()); It; ++It)
	{
		Anchor = It->GetActorTransform();
		break;
	}
	GS->SharedAnchorTransform = Anchor;
	GS->bColocationReady = true;
	UE_LOG(LogTemp, Display, TEXT("[Run] Shared colocation anchor established for co-op."));
}

void URunManager::NotifyPlayerDied()
{
	if (RunState == ERunState::GameOver)
	{
		return;
	}
	if (CountAlivePlayers() <= 0)
	{
		EndRun();
	}
}

int32 URunManager::CountAlivePlayers() const
{
	int32 Alive = 0;
	for (TActorIterator<AHazardPlayerPawn> It(GetWorld()); It; ++It)
	{
		if (UHealthComponent* HC = It->GetHealthComponent())
		{
			if (!HC->IsDead())
			{
				++Alive;
			}
		}
	}
	return Alive;
}

void URunManager::BeginNextLevel()
{
	++CurrentLevel;
	SetState(ERunState::Playing);
	float Diff = 1.f;
	if (UHazardGameInstance* GI = GetWorld() ? Cast<UHazardGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		Diff = GI->DifficultyScale;
	}
	const float Mul = (1.f + (CurrentLevel - 1) * 0.25f) * Diff;
	if (UWaveDirector* WD = GetWaveDirector())
	{
		WD->StartLevel(CurrentLevel, Mul);
	}
	PushToGameState();
	UE_LOG(LogTemp, Display, TEXT("[Run] Level %d begins (difficulty x%.2f)"), CurrentLevel, Mul);

	if (CVarAutoChest.GetValueOnGameThread() > 0)
	{
		if (UWorld* W = GetWorld())
		{
			if (AHazardGameMode* GM = W->GetAuthGameMode<AHazardGameMode>())
			{
				TSubclassOf<AChestActor> Cls = GM->ChestClass;
				if (!Cls)
				{
					Cls = AChestActor::StaticClass();
				}
				FVector Loc(150.f, 150.f, 0.f);
				for (TActorIterator<AHazardBase> It(W); It; ++It)
				{
					Loc = It->GetActorLocation() + FVector(150.f, 150.f, 0.f);
					break;
				}
				if (AChestActor* Chest = W->SpawnActor<AChestActor>(Cls, FTransform(FRotator::ZeroRotator, Loc)))
				{
					Chest->RarityTier = FMath::Clamp(CurrentLevel / 2 + 1, 1, 5); // mirror milestone scaling
					Chest->OpenDelay = 3.f;
				}
			}
		}
	}

	// Produce a fresh roller offer immediately when play resumes (don't wait up to ~10s for the timer).
	RollWeaponOffer();
}

void URunManager::HandleAllWavesCleared()
{
	if (RunState != ERunState::Playing)
	{
		return;
	}
	SetState(ERunState::ChoosingCard);
	OfferCards();
	PushToGameState();
	UE_LOG(LogTemp, Display, TEXT("[Run] Level %d cleared — choose 1 of %d cards."), CurrentLevel, CurrentOffer.Num());

	if (CVarAutoPickCards.GetValueOnGameThread() > 0 && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(AutoPickHandle, this, &URunManager::AutoPickNow, 1.5f, false);
	}
}

void URunManager::AutoPickNow()
{
	PickCard(0);
}

void URunManager::OfferCards()
{
	CurrentOffer.Reset();
	if (UCardSystem* CS = GetCardSystem())
	{
		int32 Bias = CurrentLevel / 4; // odds creep up as the run goes
		if (UWaveDirector* WD = GetWaveDirector())
		{
			if (WD->bBossSpawnedThisLevel)
			{
				Bias += 2; // post-boss levels offer rarer cards
			}
		}
		CurrentOffer = CS->OfferCards(3, Bias);
	}
}

void URunManager::PickCard(int32 Index)
{
	if (RunState != ERunState::ChoosingCard || !CurrentOffer.IsValidIndex(Index))
	{
		return;
	}
	const FCardRow Card = CurrentOffer[Index];
	ApplyCardEffect(Card);
	CurrentOffer.Reset();
	UE_LOG(LogTemp, Display, TEXT("[Run] Picked card: %s (dmg x%.2f, rof x%.2f, melee x%.2f)"),
		*Card.DisplayName, DamageMult, FireRateMult, MeleeDamageMult);
	BeginNextLevel();
}

void URunManager::ApplyCardEffect(const FCardRow& Card)
{
	switch (Card.Effect)
	{
	case ECardEffect::WeaponDamageUp:     DamageMult += Card.Magnitude; break;
	case ECardEffect::FireRateUp:         FireRateMult += Card.Magnitude; break;
	case ECardEffect::MeleeDamageUp:      MeleeDamageMult += Card.Magnitude; break;
	case ECardEffect::ProjectileSpeedUp:  ProjectileSpeedMult += Card.Magnitude; break;
	case ECardEffect::MaxHealthUp:        BonusMaxHealth += Card.Magnitude; ApplyPlayerStatCards(); break;
	case ECardEffect::AmmoCapacityUp:     BonusAmmo += FMath::RoundToInt(Card.Magnitude); break;
	case ECardEffect::LuckUp:             Luck += FMath::RoundToInt(Card.Magnitude); break;
	default: break;
	}
}

void URunManager::ApplyPlayerStatCards()
{
	for (TActorIterator<AHazardPlayerPawn> It(GetWorld()); It; ++It)
	{
		if (UHealthComponent* HC = It->GetHealthComponent())
		{
			if (HC->IsDead())
			{
				continue; // a stat card must not revive a downed player
			}
			HC->MaxHealth = 100.f + ClassHealthBonus + BonusMaxHealth; // keep the class bonus in the base
			HC->ResetToFull(); // living player: raise cap + top up
		}
	}
}

void URunManager::SaveProgress()
{
	// Lifecycle save (focus loss / headset removed): bank coins earned so far so an OS kill before
	// EndRun doesn't lose them. CommitRunCoins is delta-based, so calling it repeatedly is safe.
	CommitRunCoins();
}

void URunManager::CommitRunCoins()
{
	UWorld* W = GetWorld();
	if (!W || W->IsNetMode(NM_Client))
	{
		return; // GS->Coins is server-authoritative; only the host banks
	}
	AHazardGameState* GS = GetGS();
	UHazardGameInstance* GI = W->GetGameInstance<UHazardGameInstance>();
	if (!GS || !GI)
	{
		return;
	}
	const int32 Delta = GS->Coins - CoinsBankedThisRun;
	if (Delta > 0)
	{
		GI->BankRunResult(Delta, GS->Score, CurrentLevel);
		CoinsBankedThisRun = GS->Coins;
	}
	else
	{
		GI->UpdateRecords(GS->Score, CurrentLevel);
	}
	HighScore = GI->GetHighScore();
	GS->HighScore = HighScore;
}

void URunManager::EndRun()
{
	if (RunState == ERunState::GameOver)
	{
		return;
	}
	SetState(ERunState::GameOver);
	if (UWaveDirector* WD = GetWaveDirector())
	{
		WD->StopWaves();
	}
	GetWorld()->GetTimerManager().ClearTimer(RollerHandle);
	if (AHazardGameState* GSR = GetGS()) { GSR->bRollerActive = false; }
	CommitRunCoins(); // bank any not-yet-banked run coins (idempotent with the lifecycle save)
	PushToGameState();
	UE_LOG(LogTemp, Display, TEXT("[Run] GAME OVER at level %d."), CurrentLevel);
}

void URunManager::SetState(ERunState NewState)
{
	RunState = NewState;
	if (AHazardGameState* GS = GetGS())
	{
		GS->SetRunState(NewState);
	}
}

void URunManager::PushToGameState()
{
	if (AHazardGameState* GS = GetGS())
	{
		GS->CurrentLevel = CurrentLevel;
		GS->HighScore = HighScore;
		GS->OfferedCards.Reset();
		GS->OfferedCardRarities.Reset();
		for (const FCardRow& C : CurrentOffer)
		{
			GS->OfferedCards.Add(C.DisplayName + TEXT(" - ") + C.Description);
			GS->OfferedCardRarities.Add(static_cast<uint8>(C.Rarity));
		}
	}
}

void URunManager::LoadRecords()
{
	// Records + meta-profile are owned by the GameInstance (persists across levels).
	if (UHazardGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UHazardGameInstance>() : nullptr)
	{
		HighScore = GI->GetHighScore();
		BestLevel = GI->GetBestLevel();
	}
}

void URunManager::SaveRecordsIfBest()
{
	// Lifecycle/mid-run persistence: update records only (coins are banked once at EndRun).
	const int32 Score = GetGS() ? GetGS()->Score : 0;
	if (UHazardGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UHazardGameInstance>() : nullptr)
	{
		GI->UpdateRecords(Score, CurrentLevel);
		HighScore = GI->GetHighScore();
	}
	if (AHazardGameState* GS = GetGS())
	{
		GS->HighScore = HighScore;
	}
}

void URunManager::RollWeaponOffer()
{
	AHazardGameState* GS = GetGS();
	if (!GS)
	{
		return;
	}
	if (RunState != ERunState::Playing)
	{
		GS->bRollerActive = false;
		GS->RollerWeaponName.Reset();
		return;
	}
	const int32 Tier = FMath::Clamp(GS->CurrentWave / 10 + 1, 1, 5);
	const EItemRarity R = UWeaponLibrary::RollRarityForTier(Tier, Luck);
	CurrentRollerDef = RollUnlockedWeapon(R);
	GS->RollerWeaponName = CurrentRollerDef.DisplayName;
	GS->RollerWeaponRarity = static_cast<uint8>(CurrentRollerDef.Rarity);
	GS->bRollerActive = !CurrentRollerDef.DisplayName.IsEmpty();
	GS->bRollerClaimed = false;
	GS->RollerEndTime = GS->GetServerWorldTimeSeconds() + 10.f; // server-synced clock for the HUD countdown
	UE_LOG(LogTemp, Display, TEXT("[Roller] offer: %s [%s]"), *CurrentRollerDef.DisplayName, *UWeaponLibrary::RarityName(CurrentRollerDef.Rarity));
}

FWeaponDef URunManager::RollUnlockedWeapon(EItemRarity DesiredRarity, bool bAllowChestOnly) const
{
	const TArray<FWeaponDef> All = UWeaponLibrary::GetAllWeapons();
	auto IsAllowed = [this, bAllowChestOnly](const FWeaponDef& D)
	{
		if (D.bChestOnly) { return bAllowChestOnly; } // chest-only exclusives: only from chests, always available there
		return D.RowName == FName("Pistol") || D.RowName == FName("Blade")
			|| UnlockedWeaponRows.Contains(D.RowName);
	};
	TArray<FWeaponDef> Pool;
	for (const FWeaponDef& D : All)
	{
		if (IsAllowed(D) && D.Rarity == DesiredRarity) { Pool.Add(D); }
	}
	for (int32 r = static_cast<int32>(DesiredRarity); r >= 0 && Pool.Num() == 0; --r)
	{
		for (const FWeaponDef& D : All)
		{
			if (IsAllowed(D) && static_cast<int32>(D.Rarity) == r) { Pool.Add(D); }
		}
	}
	if (Pool.Num() == 0)
	{
		FWeaponDef Fallback;
		Fallback.DisplayName = TEXT("Pistol");
		Fallback.Rarity = EItemRarity::Common;
		return Fallback;
	}
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

void URunManager::ClaimRolledWeapon(AHazardPlayerPawn* Pawn)
{
	AHazardGameState* GS = GetGS();
	if (!GS || GS->RunState != ERunState::Playing || !GS->bRollerActive || GS->bRollerClaimed || !Pawn)
	{
		return;
	}
	GS->bRollerClaimed = true;
	Pawn->EquipToArsenal(CurrentRollerDef);
	UE_LOG(LogTemp, Display, TEXT("[Roller] %s claimed %s [%s]"), *Pawn->GetName(), *CurrentRollerDef.DisplayName, *UWeaponLibrary::RarityName(CurrentRollerDef.Rarity));
}

UWaveDirector* URunManager::GetWaveDirector() const
{
	if (UWorld* W = GetWorld())
	{
		if (AHazardGameMode* GM = W->GetAuthGameMode<AHazardGameMode>())
		{
			return GM->WaveDirector;
		}
	}
	return nullptr;
}

AHazardGameState* URunManager::GetGS() const
{
	return GetWorld() ? GetWorld()->GetGameState<AHazardGameState>() : nullptr;
}

UCardSystem* URunManager::GetCardSystem() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UCardSystem>() : nullptr;
}
