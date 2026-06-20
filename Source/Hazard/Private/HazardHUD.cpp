#include "HazardHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HazardGameState.h"
#include "HazardPlayerPawn.h"
#include "HazardPlayerController.h"
#include "HealthComponent.h"
#include "WeaponComponent.h"
#include "WeaponBase.h"
#include "WeaponLibrary.h"

void AHazardHUD::DrawHUD()
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
	const FLinearColor Red(0.95f, 0.25f, 0.25f, 1.f);
	const FLinearColor PanelBG(0.02f, 0.03f, 0.05f, 0.55f);
	const FLinearColor BarBG(0.10f, 0.10f, 0.12f, 0.85f);

	auto Panel = [&](float X, float Y, float W, float H)
	{
		DrawRect(PanelBG, X, Y, W, H);
		DrawRect(FLinearColor(0.9f, 0.2f, 0.2f, 0.5f), X, Y, W, 3.f); // accent stripe
	};
	auto Label = [&](const FString& S, const FLinearColor& C, float X, float Y, float Scale)
	{
		DrawText(S, C, X, Y, Font, Scale, false);
	};
	auto Bar = [&](float X, float Y, float W, float H, float Pct, const FLinearColor& Col)
	{
		DrawRect(BarBG, X, Y, W, H);
		DrawRect(Col, X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), H);
	};

	const AHazardGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHazardGameState>() : nullptr;
	const APlayerController* PC = GetOwningPlayerController();
	const AHazardPlayerController* HPC = Cast<AHazardPlayerController>(PC);
	AHazardPlayerPawn* Pawn = PC ? Cast<AHazardPlayerPawn>(PC->GetPawn()) : nullptr;

	// --- Top-left status panel ---
	const float PX = 28.f, PY = 28.f, PW = 330.f;
	Panel(PX, PY, PW, 188.f);
	float X = PX + 16.f;
	float Y = PY + 14.f;
	Label(TEXT("HAZARD"), Red, X, Y, 1.4f); Y += 34.f;

	if (Pawn)
	{
		if (UHealthComponent* HC = Pawn->GetHealthComponent())
		{
			const float P = HC->GetHealthPercent();
			Bar(X, Y + 4.f, PW - 32.f, 16.f, P, FLinearColor(1.f - P, 0.2f + 0.6f * P, 0.15f, 1.f));
			Label(FString::Printf(TEXT("HP  %.0f / %.0f"), HC->GetHealth(), HC->MaxHealth), White, X + 6.f, Y + 2.f, 0.85f);
			Y += 26.f;
		}
		if (UWeaponComponent* WC = Pawn->GetRightWeapon())
		{
			if (AWeaponBase* W = WC->GetWeapon())
			{
				if (W->IsRanged())
				{
					const FString Ammo = W->IsReloading() ? FString(TEXT("RELOADING")) : FString::Printf(TEXT("%d / %d"), W->GetCurrentAmmo(), W->GetMaxAmmo());
					Label(FString::Printf(TEXT("AMMO  %s"), *Ammo), Amber, X, Y, 0.95f);
					Y += 24.f;
				}
			}
		}
	}

	if (GS)
	{
		const float P = GS->BaseHealthPercent;
		Bar(X, Y + 4.f, PW - 32.f, 14.f, P, FLinearColor(0.2f, 0.6f, 1.f, 1.f));
		Label(FString::Printf(TEXT("BASE  %.0f%%"), P * 100.f), White, X + 6.f, Y + 1.f, 0.8f);
		Y += 26.f;
		Label(FString::Printf(TEXT("Level %d    Wave %d    x%d alive"), GS->CurrentLevel, GS->CurrentWave, GS->EnemiesAlive), White, X, Y, 0.9f); Y += 22.f;
		Label(FString::Printf(TEXT("Score %d    Coins "), GS->Score), White, X, Y, 0.9f);
		Label(FString::Printf(TEXT("%d"), GS->Coins), Amber, X + 210.f, Y, 0.9f);
	}

	// --- Crosshair (during play) ---
	if (GS && GS->RunState == ERunState::Playing)
	{
		const float CX = SW * 0.5f, CY = SH * 0.5f;
		const FLinearColor CH(1.f, 1.f, 1.f, 0.7f);
		DrawRect(CH, CX - 9.f, CY - 1.f, 7.f, 2.f);
		DrawRect(CH, CX + 2.f, CY - 1.f, 7.f, 2.f);
		DrawRect(CH, CX - 1.f, CY - 9.f, 2.f, 7.f);
		DrawRect(CH, CX - 1.f, CY + 2.f, 2.f, 7.f);
	}

	// --- Weapon RNG roller (top-center, "near the wall"): claim the offer with [G] / grip-grab ---
	if (GS && GS->RunState == ERunState::Playing && GS->bRollerActive)
	{
		const float RW = 360.f, RH = 56.f;
		const float RX = SW * 0.5f - RW * 0.5f;
		const float RY = 24.f;
		const FLinearColor RCol = UWeaponLibrary::RarityColor(static_cast<EItemRarity>(GS->RollerWeaponRarity));
		DrawRect(FLinearColor(0.03f, 0.04f, 0.06f, 0.88f), RX, RY, RW, RH);
		DrawRect(RCol, RX, RY, RW, 4.f); // rarity bar
		Label(TEXT("WEAPON  RNG"), Amber, RX + 14.f, RY + 8.f, 0.7f);
		Label(GS->RollerWeaponName, RCol, RX + 14.f, RY + 26.f, 1.05f);
		if (GS->bRollerClaimed)
		{
			Label(TEXT("TAKEN"), FLinearColor(0.5f, 0.9f, 0.5f, 1.f), RX + RW - 84.f, RY + 24.f, 0.95f);
		}
		else
		{
			const float Remaining = FMath::Max(0.f, GS->RollerEndTime - GS->GetServerWorldTimeSeconds());
			Label(FString::Printf(TEXT("[G] take  %.0fs"), Remaining), White, RX + RW - 156.f, RY + 24.f, 0.85f);
		}
	}

	// --- Weapon slots (bottom-center) ---
	if (GS && GS->RunState == ERunState::Playing && Pawn && Pawn->ArsenalNames.Num() > 0)
	{
		const int32 N = Pawn->ArsenalNames.Num();
		const float SlotW = 168.f, SlotH = 40.f, Gap = 10.f;
		const float TotalW = N * SlotW + (N - 1) * Gap;
		float SX = SW * 0.5f - TotalW * 0.5f;
		const float SY = SH - 72.f;
		Label(TEXT("[Q] swap"), FLinearColor(0.6f, 0.6f, 0.62f), SW * 0.5f - 36.f, SY - 22.f, 0.7f);
		for (int32 i = 0; i < N; ++i)
		{
			const bool bActive = (i == Pawn->ActiveSlot);
			FLinearColor Col = White;
			if (Pawn->ArsenalRarities.IsValidIndex(i))
			{
				Col = UWeaponLibrary::RarityColor(static_cast<EItemRarity>(Pawn->ArsenalRarities[i]));
			}
			DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, bActive ? 0.95f : 0.6f), SX, SY, SlotW, SlotH);
			DrawRect(Col, SX, SY, SlotW, 4.f); // rarity stripe
			if (bActive)
			{
				DrawRect(Amber, SX, SY, SlotW, 2.f);
				DrawRect(Amber, SX, SY + SlotH - 2.f, SlotW, 2.f);
				DrawRect(Amber, SX, SY, 2.f, SlotH);
				DrawRect(Amber, SX + SlotW - 2.f, SY, 2.f, SlotH);
			}
			Label(FString::Printf(TEXT("%d  %s"), i + 1, *Pawn->ArsenalNames[i]), bActive ? White : FLinearColor(0.7f, 0.7f, 0.72f), SX + 12.f, SY + 12.f, 0.8f);
			SX += SlotW + Gap;
		}
	}

	// --- Centered overlay helper (dim + panel) ---
	auto Overlay = [&](float BoxW, float BoxH) -> FVector2D
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, SW, SH);
		const float BX = SW * 0.5f - BoxW * 0.5f;
		const float BY = SH * 0.5f - BoxH * 0.5f;
		DrawRect(FLinearColor(0.03f, 0.04f, 0.06f, 0.92f), BX, BY, BoxW, BoxH);
		DrawRect(FLinearColor(0.9f, 0.2f, 0.2f, 0.8f), BX, BY, BoxW, 4.f);
		return FVector2D(BX, BY);
	};

	if (GS && GS->RunState == ERunState::ChoosingCard)
	{
		const FVector2D B = Overlay(620.f, 320.f);
		float CY = B.Y + 26.f;
		Label(TEXT("LEVEL CLEARED  -  CHOOSE A CARD"), Amber, B.X + 30.f, CY, 1.4f); CY += 56.f;
		for (int32 i = 0; i < GS->OfferedCards.Num(); ++i)
		{
			FLinearColor CardCol = White;
			if (GS->OfferedCardRarities.IsValidIndex(i))
			{
				CardCol = UWeaponLibrary::RarityColor(static_cast<EItemRarity>(GS->OfferedCardRarities[i]));
			}
			DrawRect(FLinearColor(0.10f, 0.12f, 0.16f, 0.9f), B.X + 30.f, CY - 4.f, 560.f, 44.f);
			DrawRect(CardCol, B.X + 30.f, CY - 4.f, 7.f, 44.f); // rarity stripe
			Label(FString::Printf(TEXT("[%d]   %s"), i + 1, *GS->OfferedCards[i]), CardCol, B.X + 48.f, CY + 6.f, 1.0f);
			CY += 56.f;
		}
		Label(TEXT("Press 1 / 2 / 3"), Amber, B.X + 30.f, CY + 6.f, 1.0f);
	}
	else if (GS && GS->RunState == ERunState::GameOver)
	{
		const FVector2D B = Overlay(560.f, 230.f);
		Label(TEXT("GAME OVER"), Red, B.X + 40.f, B.Y + 34.f, 2.4f);
		Label(FString::Printf(TEXT("Final Score: %d        Best: %d"), GS->Score, GS->HighScore), White, B.X + 40.f, B.Y + 110.f, 1.2f);
		Label(TEXT("[R] Restart        [M] Main Menu"), Amber, B.X + 40.f, B.Y + 160.f, 1.1f);
	}

	if (HPC && HPC->bPaused)
	{
		const FVector2D B = Overlay(520.f, 180.f);
		Label(TEXT("PAUSED"), White, B.X + 40.f, B.Y + 34.f, 2.2f);
		Label(TEXT("[Esc/P] Resume        [M] Main Menu"), Amber, B.X + 40.f, B.Y + 110.f, 1.1f);
	}
}
