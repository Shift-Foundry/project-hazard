#include "HazardGameInstance.h"
#include "RunManager.h"
#include "HazardSaveGame.h"
#include "HazardPlayerController.h"
#include "HazardGameState.h"
#include "HazardTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"

static const TCHAR* GHazardSaveSlot = TEXT("HazardRun");

void UHazardGameInstance::Init()
{
	Super::Init();
	LoadProfile();
	// On a standalone XR headset, focus loss (system menu / headset removed / backgrounded) is
	// frequent — bind it to pause + persist so the run survives an OS kill.
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UHazardGameInstance::HandleAppDeactivate);
	ReactivateHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UHazardGameInstance::HandleAppReactivate);
}

void UHazardGameInstance::Shutdown()
{
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(ReactivateHandle);
	Super::Shutdown();
}

void UHazardGameInstance::HandleAppDeactivate()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	if (URunManager* RM = W->GetSubsystem<URunManager>())
	{
		RM->SaveProgress();
	}
	APlayerController* PC = GetFirstLocalPlayerController(W);
	if (AHazardPlayerController* HPC = Cast<AHazardPlayerController>(PC))
	{
		if (!HPC->bPaused) // don't clobber a manual pause
		{
			HPC->bPaused = true;
			HPC->SetPause(true);
			HPC->bShowMouseCursor = true;
			bPausedByLifecycle = true;
		}
	}
	else if (PC)
	{
		PC->SetPause(true);
		bPausedByLifecycle = true;
	}
	UE_LOG(LogTemp, Display, TEXT("[Lifecycle] App deactivated -> run persisted + paused."));
}

void UHazardGameInstance::HandleAppReactivate()
{
	UWorld* W = GetWorld();
	if (W && bPausedByLifecycle) // only undo the pause WE applied
	{
		APlayerController* PC = GetFirstLocalPlayerController(W);
		if (AHazardPlayerController* HPC = Cast<AHazardPlayerController>(PC))
		{
			HPC->bPaused = false;
			HPC->SetPause(false);
			HPC->bShowMouseCursor = false;
		}
		else if (PC)
		{
			PC->SetPause(false);
		}
		bPausedByLifecycle = false;
	}
	UE_LOG(LogTemp, Display, TEXT("[Lifecycle] App reactivated."));
}

void UHazardGameInstance::SetDifficultyIndex(int32 Index)
{
	DifficultyIndex = FMath::Clamp(Index, 0, 2);
	switch (DifficultyIndex)
	{
	case 0: DifficultyScale = 0.75f; break; // Easy
	case 2: DifficultyScale = 1.5f;  break; // Hard
	default: DifficultyScale = 1.0f; break;  // Normal
	}
}

FString UHazardGameInstance::GetDifficultyName() const
{
	switch (DifficultyIndex)
	{
	case 0: return TEXT("Easy");
	case 2: return TEXT("Hard");
	default: return TEXT("Normal");
	}
}

void UHazardGameInstance::LoadProfile()
{
	bool bHadSave = false;
	if (UHazardSaveGame* SG = Cast<UHazardSaveGame>(UGameplayStatics::LoadGameFromSlot(GHazardSaveSlot, 0)))
	{
		HighScore = SG->HighScore;
		BestLevel = SG->BestLevel;
		CoinsBanked = SG->CoinsBanked;
		UnlockedWeapons = SG->UnlockedWeapons;
		UnlockedClasses = SG->UnlockedClasses;
		SelectedClass = SG->SelectedClass;
		bHadSave = true;
	}
	EnsureDefaults();
	if (!bHadSave)
	{
		SaveProfile(); // first launch: persist the seeded defaults deterministically
	}
}

void UHazardGameInstance::EnsureDefaults()
{
	auto AddUnique = [](TArray<FName>& A, FName N) { if (!A.Contains(N)) { A.Add(N); } };
	AddUnique(UnlockedWeapons, FName("Pistol"));
	AddUnique(UnlockedWeapons, FName("Blade"));
	AddUnique(UnlockedClasses, FName("Recruit"));
	if (SelectedClass.IsNone() || !UnlockedClasses.Contains(SelectedClass))
	{
		SelectedClass = FName("Recruit");
	}
}

void UHazardGameInstance::SaveProfile()
{
	UHazardSaveGame* SG = Cast<UHazardSaveGame>(UGameplayStatics::CreateSaveGameObject(UHazardSaveGame::StaticClass()));
	if (!SG)
	{
		return;
	}
	SG->HighScore = HighScore;
	SG->BestLevel = BestLevel;
	SG->CoinsBanked = CoinsBanked;
	SG->UnlockedWeapons = UnlockedWeapons;
	SG->UnlockedClasses = UnlockedClasses;
	SG->SelectedClass = SelectedClass;
	UGameplayStatics::SaveGameToSlot(SG, GHazardSaveSlot, 0);
}

bool UHazardGameInstance::TryBuyWeapon(FName Row, int32 Cost)
{
	if (Row.IsNone() || Cost <= 0 || UnlockedWeapons.Contains(Row) || CoinsBanked < Cost)
	{
		return false;
	}
	CoinsBanked -= Cost;
	UnlockedWeapons.Add(Row);
	SaveProfile();
	return true;
}

bool UHazardGameInstance::TryBuyClass(FName Row, int32 Cost)
{
	if (Row.IsNone() || Cost <= 0 || UnlockedClasses.Contains(Row) || CoinsBanked < Cost)
	{
		return false;
	}
	CoinsBanked -= Cost;
	UnlockedClasses.Add(Row);
	SaveProfile();
	return true;
}

bool UHazardGameInstance::SetSelectedClass(FName Row)
{
	if (!UnlockedClasses.Contains(Row))
	{
		return false;
	}
	SelectedClass = Row;
	SaveProfile();
	return true;
}

void UHazardGameInstance::UpdateRecords(int32 Score, int32 Level)
{
	bool bDirty = false;
	if (Score > HighScore) { HighScore = Score; bDirty = true; }
	if (Level > BestLevel) { BestLevel = Level; bDirty = true; }
	if (bDirty) { SaveProfile(); }
}

void UHazardGameInstance::BankRunResult(int32 CoinsEarned, int32 Score, int32 Level)
{
	CoinsBanked += FMath::Max(0, CoinsEarned);
	if (Score > HighScore) { HighScore = Score; }
	if (Level > BestLevel) { BestLevel = Level; }
	SaveProfile();
	UE_LOG(LogTemp, Display, TEXT("[Profile] Banked %d coins (wallet=%d). HS=%d BestLvl=%d."), CoinsEarned, CoinsBanked, HighScore, BestLevel);
}
