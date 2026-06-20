#include "HazardPlayerController.h"
#include "RunManager.h"
#include "HazardGameMode.h"
#include "HazardGameState.h"
#include "HazardPlayerPawn.h"
#include "ChestActor.h"
#include "HazardTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

AHazardPlayerController::AHazardPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC(TEXT("/Game/XRFramework/Input/IMC_Menu.IMC_Menu"));
	if (IMC.Succeeded()) { MenuMappingContext = IMC.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> MR(TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Toggle_Right.IA_Menu_Toggle_Right"));
	if (MR.Succeeded()) { MenuToggleRightAction = MR.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> ML(TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Toggle_Left.IA_Menu_Toggle_Left"));
	if (ML.Succeeded()) { MenuToggleLeftAction = ML.Object; }
}

void AHazardPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHazardPlayerController::TogglePause);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AHazardPlayerController::TogglePause);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AHazardPlayerController::OnRestart);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AHazardPlayerController::OnToMenu);

	// Enhanced Input: a controller "menu" button toggles pause (additive to legacy Esc/P).
	if (MenuMappingContext)
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
			{
				Subsys->AddMappingContext(MenuMappingContext, 0);
			}
		}
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MenuToggleRightAction) { EIC->BindAction(MenuToggleRightAction, ETriggerEvent::Started, this, &AHazardPlayerController::TogglePause); }
		if (MenuToggleLeftAction)  { EIC->BindAction(MenuToggleLeftAction,  ETriggerEvent::Started, this, &AHazardPlayerController::TogglePause); }
	}
}

void AHazardPlayerController::TogglePause()
{
	bPaused = !bPaused;
	SetPause(bPaused);
	bShowMouseCursor = bPaused;
}

void AHazardPlayerController::OnRestart()
{
	// Restart only makes sense once the run is over.
	const AHazardGameState* GS = GetWorld() ? GetWorld()->GetGameState<AHazardGameState>() : nullptr;
	if (GS && GS->RunState == ERunState::GameOver)
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}

void AHazardPlayerController::OnToMenu()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_MainMenu")));
}

void AHazardPlayerController::ServerPickCard_Implementation(int32 Index)
{
	if (UWorld* W = GetWorld())
	{
		if (URunManager* RM = W->GetSubsystem<URunManager>())
		{
			RM->PickCard(Index);
		}
	}
}

void AHazardPlayerController::ServerPlaceChest_Implementation(FVector Location)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	// Only place chests during active play, and rate-limit per player to prevent loot spam.
	URunManager* RM = W->GetSubsystem<URunManager>();
	if (!RM || RM->GetRunState() != ERunState::Playing)
	{
		return;
	}
	const float Now = W->GetTimeSeconds();
	if (Now - LastChestPlaceTime < 2.0f)
	{
		return;
	}
	LastChestPlaceTime = Now;

	// Validate client-supplied location: reject NaN, require it near the player, snap to the floor.
	if (Location.ContainsNaN())
	{
		return;
	}
	if (const APawn* P = GetPawn())
	{
		if (FVector::Dist2D(P->GetActorLocation(), Location) > 600.f)
		{
			return;
		}
	}
	Location.Z = 0.f;

	TSubclassOf<AChestActor> Cls = AChestActor::StaticClass();
	if (AHazardGameMode* GM = W->GetAuthGameMode<AHazardGameMode>())
	{
		if (GM->ChestClass)
		{
			Cls = GM->ChestClass;
		}
	}
	W->SpawnActor<AChestActor>(Cls, FTransform(FRotator::ZeroRotator, Location));
}

void AHazardPlayerController::ServerCycleWeapon_Implementation()
{
	if (AHazardPlayerPawn* P = Cast<AHazardPlayerPawn>(GetPawn()))
	{
		P->CycleWeapon();
	}
}

void AHazardPlayerController::ServerClaimRolledWeapon_Implementation()
{
	if (UWorld* W = GetWorld())
	{
		if (URunManager* RM = W->GetSubsystem<URunManager>())
		{
			RM->ClaimRolledWeapon(Cast<AHazardPlayerPawn>(GetPawn()));
		}
	}
}
