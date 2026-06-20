#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HazardPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * Routes player intents (card choice, chest placement) to the server-authoritative systems
 * via reliable Server RPCs, so the same input path works in single-player and co-op.
 */
UCLASS()
class HAZARD_API AHazardPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHazardPlayerController();

	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerPickCard(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerPlaceChest(FVector Location);

	UFUNCTION(Server, Reliable)
	void ServerCycleWeapon();

	/** Claim the current in-run weapon-roller offer into the arsenal. */
	UFUNCTION(Server, Reliable)
	void ServerClaimRolledWeapon();

	/** Drawn by AHazardHUD as a pause overlay. */
	UPROPERTY(BlueprintReadOnly, Category = "Hazard")
	bool bPaused = false;

protected:
	void TogglePause();
	void OnRestart();
	void OnToMenu();

private:
	float LastChestPlaceTime = -100.f;

	// Enhanced Input: pause via a controller "menu" button (additive; legacy Esc/P stay for desktop).
	UPROPERTY()
	TObjectPtr<UInputMappingContext> MenuMappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MenuToggleRightAction;

	UPROPERTY()
	TObjectPtr<UInputAction> MenuToggleLeftAction;
};
