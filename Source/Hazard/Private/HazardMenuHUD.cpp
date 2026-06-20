#include "HazardMenuHUD.h"
#include "HazardMenuController.h"
#include "HazardGameInstance.h"
#include "WeaponLibrary.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	FString GraphicsTierName(int32 Level)
	{
		switch (Level)
		{
		case 0: return TEXT("Low");
		case 1: return TEXT("Medium");
		case 2: return TEXT("High");
		default: return TEXT("Epic");
		}
	}
}

void AHazardMenuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	const float SW = Canvas->SizeX;
	const float SH = Canvas->SizeY;

	const FLinearColor White(0.95f, 0.95f, 0.97f, 1.f);
	const FLinearColor Amber(1.f, 0.78f, 0.25f, 1.f);
	const FLinearColor Red(0.92f, 0.22f, 0.22f, 1.f);
	const FLinearColor Dim(0.55f, 0.57f, 0.62f, 1.f);

	// Background wash + title bar.
	DrawRect(FLinearColor(0.015f, 0.02f, 0.03f, 1.f), 0.f, 0.f, SW, SH);
	DrawRect(FLinearColor(0.06f, 0.02f, 0.03f, 1.f), 0.f, SH * 0.16f, SW, 4.f);

	auto Center = [&](const FString& S, const FLinearColor& C, float Y, float Scale)
	{
		float TW, TH;
		Canvas->TextSize(Font, S, TW, TH, Scale, Scale);
		DrawText(S, C, SW * 0.5f - TW * 0.5f, Y, Font, Scale, false);
	};

	Center(TEXT("H A Z A R D"), Red, SH * 0.10f, 3.0f);
	Center(TEXT("room-scale MR roguelite survivor"), Dim, SH * 0.10f + 70.f, 1.0f);

	const AHazardMenuController* PC = Cast<AHazardMenuController>(GetOwningPlayerController());
	const EMenuScreen Screen = PC ? PC->Screen : EMenuScreen::Main;

	// Centered options panel.
	const float PW = 560.f, PH = 320.f;
	const float PX = SW * 0.5f - PW * 0.5f;
	const float PY = SH * 0.40f;
	DrawRect(FLinearColor(0.03f, 0.04f, 0.06f, 0.85f), PX, PY, PW, PH);
	DrawRect(FLinearColor(0.9f, 0.2f, 0.2f, 0.6f), PX, PY, PW, 4.f);

	float Y = PY + 34.f;
	const float X = PX + 48.f;
	auto Opt = [&](const FString& S, const FLinearColor& C)
	{
		DrawText(S, C, X, Y, Font, 1.35f, false);
		Y += 48.f;
	};

	if (Screen == EMenuScreen::Main)
	{
		Opt(TEXT("[1]   Play  -  PC preview room"), White);
		Opt(TEXT("[2]   Play  -  VR arena"), White);
		Opt(TEXT("[6]   Shop / Loadout"), White);
		Opt(TEXT("[3]   Settings"), White);
		Opt(TEXT("[5]   Story  (coming soon)"), Dim);
		Opt(TEXT("[4]   Quit"), White);
	}
	else if (Screen == EMenuScreen::Settings)
	{
		FString DiffName(TEXT("Normal"));
		if (const UHazardGameInstance* GI = GetWorld() ? Cast<UHazardGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
		{
			DiffName = GI->GetDifficultyName();
		}
		Opt(TEXT("SETTINGS"), Amber);
		Opt(FString::Printf(TEXT("[1]   Graphics:   %s"), *GraphicsTierName(PC ? PC->GraphicsLevel : 2)), White);
		Opt(FString::Printf(TEXT("[2]   Difficulty: %s"), *DiffName), White);
		Opt(TEXT("[Esc] Back"), Dim);
	}
	else // Shop
	{
		const UHazardGameInstance* GI = GetWorld() ? Cast<UHazardGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
		const int32 Coins = GI ? GI->GetCoins() : 0;
		const FName Sel = GI ? GI->GetSelectedClass() : NAME_None;
		DrawText(FString::Printf(TEXT("SHOP    Coins: %d"), Coins), Amber, X, Y, Font, 1.2f, false);
		Y += 38.f;

		TArray<FHazardShopEntry> Entries;
		AHazardMenuController::BuildShopEntries(Entries);
		const int32 Cursor = PC ? PC->ShopCursor : 0;
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			const FHazardShopEntry& E = Entries[i];
			const bool bHi = (i == Cursor);
			FLinearColor C = White;
			FString State;
			if (E.bIsClass)
			{
				const bool bOwned = GI && GI->IsClassUnlocked(E.Row);
				if (E.Row == Sel) { State = TEXT("SELECTED"); C = Amber; }
				else if (bOwned) { State = TEXT("owned - Enter=select"); }
				else { State = FString::Printf(TEXT("%d coins"), E.Cost); C = (Coins >= E.Cost) ? White : Dim; }
			}
			else
			{
				const bool bOwned = GI && GI->IsWeaponUnlocked(E.Row);
				C = (bOwned || Coins >= E.Cost) ? UWeaponLibrary::RarityColor(static_cast<EItemRarity>(E.Rarity)) : Dim;
				State = bOwned ? FString(TEXT("owned")) : FString::Printf(TEXT("%d coins"), E.Cost);
			}
			if (bHi) { DrawRect(FLinearColor(0.12f, 0.14f, 0.18f, 0.9f), PX + 10.f, Y - 2.f, PW - 20.f, 26.f); }
			DrawText(FString::Printf(TEXT("%s%s  %s"), bHi ? TEXT("> ") : TEXT("  "), E.bIsClass ? TEXT("CLASS") : TEXT(" WPN "), *E.Name), C, X, Y, Font, 0.95f, false);
			DrawText(State, C, PX + PW - 210.f, Y, Font, 0.85f, false);
			Y += 27.f;
		}
		DrawText(TEXT("[Up/Down] move    [Enter] buy/select    [Esc] back"), Dim, X, PY + PH - 28.f, Font, 0.8f, false);
	}

	Center(TEXT("PC: WASD move, mouse aim, LMB fire, RMB melee, 1/2/3 cards, C chest"), Dim, SH - 60.f, 0.9f);
}
