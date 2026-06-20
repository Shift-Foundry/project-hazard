#include "HazardMenuController.h"
#include "HazardGameInstance.h"
#include "WeaponLibrary.h"
#include "ClassLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"

void AHazardMenuController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI());

	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GraphicsLevel = FMath::Clamp(GUS->GetOverallScalabilityLevel(), 0, 3);
	}
}

void AHazardMenuController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AHazardMenuController::OnKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AHazardMenuController::OnKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AHazardMenuController::OnKey3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AHazardMenuController::OnKey4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AHazardMenuController::OnKey5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AHazardMenuController::OnKey6);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHazardMenuController::OnBack);

	// Shop navigation (up/down to move, Enter to buy/select).
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AHazardMenuController::OnShopUp);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AHazardMenuController::OnShopUp);
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AHazardMenuController::OnShopDown);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AHazardMenuController::OnShopDown);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AHazardMenuController::OnShopConfirm);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHazardMenuController::OnShopConfirm);
}

void AHazardMenuController::BuildShopEntries(TArray<FHazardShopEntry>& Out)
{
	Out.Reset();
	for (const FWeaponDef& D : UWeaponLibrary::GetShopWeapons())
	{
		FHazardShopEntry E;
		E.Row = D.RowName;
		E.bIsClass = false;
		E.Name = D.DisplayName;
		E.Cost = D.CoinCost;
		E.Rarity = static_cast<uint8>(D.Rarity);
		Out.Add(E);
	}
	TArray<FName> Rows;
	TArray<FClassDef> Defs;
	UClassLibrary::GetAllClasses(Rows, Defs);
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		FHazardShopEntry E;
		E.Row = Rows[i];
		E.bIsClass = true;
		E.Name = Defs[i].DisplayName;
		E.Desc = Defs[i].Description;
		E.Cost = Defs[i].CoinCost;
		Out.Add(E);
	}
}

void AHazardMenuController::OnKey6()
{
	if (Screen == EMenuScreen::Main)
	{
		Screen = EMenuScreen::Shop;
		ShopCursor = 0;
	}
}

void AHazardMenuController::OnShopUp()
{
	if (Screen == EMenuScreen::Shop)
	{
		ShopCursor = FMath::Max(0, ShopCursor - 1);
	}
}

void AHazardMenuController::OnShopDown()
{
	if (Screen == EMenuScreen::Shop)
	{
		TArray<FHazardShopEntry> E;
		BuildShopEntries(E);
		ShopCursor = FMath::Clamp(ShopCursor + 1, 0, FMath::Max(0, E.Num() - 1));
	}
}

void AHazardMenuController::OnShopConfirm()
{
	if (Screen != EMenuScreen::Shop)
	{
		return;
	}
	TArray<FHazardShopEntry> E;
	BuildShopEntries(E);
	UHazardGameInstance* GI = Cast<UHazardGameInstance>(GetGameInstance());
	if (!GI || !E.IsValidIndex(ShopCursor))
	{
		return;
	}
	const FHazardShopEntry& Ent = E[ShopCursor];
	if (Ent.bIsClass)
	{
		if (GI->IsClassUnlocked(Ent.Row)) { GI->SetSelectedClass(Ent.Row); }
		else { GI->TryBuyClass(Ent.Row, Ent.Cost); }
	}
	else if (!GI->IsWeaponUnlocked(Ent.Row))
	{
		GI->TryBuyWeapon(Ent.Row, Ent.Cost);
	}
}

void AHazardMenuController::OnKey1()
{
	if (Screen == EMenuScreen::Main)
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("L_PreviewRoom"))); // Play (PC preview room)
	}
	else // Settings: cycle graphics
	{
		GraphicsLevel = (GraphicsLevel + 1) % 4;
		ApplyGraphics();
	}
}

void AHazardMenuController::OnKey2()
{
	if (Screen == EMenuScreen::Main)
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("L_HazardArena"))); // Play (VR arena)
	}
	else if (UHazardGameInstance* GI = Cast<UHazardGameInstance>(GetGameInstance())) // Settings: cycle difficulty
	{
		GI->SetDifficultyIndex((GI->DifficultyIndex + 1) % 3);
	}
}

void AHazardMenuController::OnKey3()
{
	if (Screen == EMenuScreen::Main)
	{
		Screen = EMenuScreen::Settings;
	}
}

void AHazardMenuController::OnKey4()
{
	if (Screen == EMenuScreen::Main)
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

void AHazardMenuController::OnKey5()
{
	// Story mode placeholder — content TBD.
	UE_LOG(LogTemp, Display, TEXT("[Menu] Story mode selected — coming soon."));
}

void AHazardMenuController::OnBack()
{
	if (Screen == EMenuScreen::Settings || Screen == EMenuScreen::Shop)
	{
		Screen = EMenuScreen::Main;
	}
}

void AHazardMenuController::ApplyGraphics()
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetOverallScalabilityLevel(GraphicsLevel);
		GUS->ApplySettings(false);
		GUS->SaveSettings();
	}
}
