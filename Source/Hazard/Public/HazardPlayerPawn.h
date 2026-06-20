#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HazardTypes.h"
#include "HazardPlayerPawn.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UShiftXRHandInteractor;
class UHealthComponent;
class UWeaponComponent;
class UColocationComponent;
class UMotionControllerComponent;
class UInputMappingContext;
class UInputAction;

/**
 * Room-scale XR pawn: HMD-locked camera + two hands (UShiftXRHandInteractor) each carrying a
 * UWeaponComponent (right = ranged, left = melee), plus a replicated HealthComponent. Fires via
 * hand gestures (VR) or mouse (desktop). No artificial locomotion — the player physically moves.
 */
UCLASS()
class HAZARD_API AHazardPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	AHazardPlayerPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	USceneComponent* VROrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	USceneComponent* LeftHandRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	USceneComponent* RightHandRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UStaticMeshComponent* LeftHandMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UStaticMeshComponent* RightHandMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UShiftXRHandInteractor* LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UShiftXRHandInteractor* RightHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UHealthComponent* Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UWeaponComponent* RightWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UWeaponComponent* LeftWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UColocationComponent* Colocation;

	// OpenXR controller aim-pose sources (used for aim when a 6DoF controller is tracked, e.g. on
	// XREAL Aura). On a hands-only device (Galaxy XR) these stay untracked and aim uses the hand ray.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UMotionControllerComponent* RightAim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	UMotionControllerComponent* LeftAim;

	UFUNCTION(BlueprintPure, Category = "Hazard")
	UHealthComponent* GetHealthComponent() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Hazard")
	UWeaponComponent* GetRightWeapon() const { return RightWeapon; }

	UFUNCTION(BlueprintPure, Category = "Hazard")
	UWeaponComponent* GetLeftWeapon() const { return LeftWeapon; }

	/** Desktop flat-screen control speed (cm/s) used when no HMD is present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard")
	float FlatMoveSpeed = 350.f;

	UFUNCTION(BlueprintPure, Category = "Hazard")
	bool IsFlatMode() const { return bFlatMode; }

	// --- Arsenal (weapon slots) ---

	/** Max weapon slots. Default 2; a class perk can raise this (e.g. to 3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard|Arsenal")
	int32 MaxWeaponSlots = 2;

	/** Server: add a rolled weapon to the arsenal (fills a free slot, else replaces the active one) and equip it. */
	void EquipToArsenal(const FWeaponDef& Def);

	/** Server: switch to the next weapon slot and equip it. */
	void CycleWeapon();

	/** HUD mirror: weapon display names per slot (replicated). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Arsenal")
	TArray<FString> ArsenalNames;

	/** HUD mirror: weapon rarity per slot, as EItemRarity (replicated). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Arsenal")
	TArray<uint8> ArsenalRarities;

	/** Currently selected slot (replicated). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hazard|Arsenal")
	int32 ActiveSlot = 0;

private:
	void UpdateHand(UShiftXRHandInteractor* HandComp, USceneComponent* HandRoot);
	void FireRightPressed();
	void MeleeLeftPressed();
	void OnPickCard0();
	void OnPickCard1();
	void OnPickCard2();
	void OnPlaceChest();
	void PickCardIndex(int32 Index);
	void OnCycleWeaponInput();
	void OnClaimRolledWeapon();
	void EquipActiveToHand();
	void RefreshArsenalMirror();

	/** Server-side authoritative weapon defs, one per slot. */
	TArray<FWeaponDef> Arsenal;

	// Flat (desktop, no-HMD) controls
	void MoveForwardInput(float Value);
	void MoveRightInput(float Value);
	void TurnInput(float Value);
	void LookUpInput(float Value);

	UFUNCTION()
	void HandlePlayerDeath(AActor* Killer);

	// --- Enhanced Input (XR controllers; hand pinch fires via the gesture path) ---
	// Hands-first design for Android XR (Galaxy XR = hands/eyes; Aura = controller+hands):
	// pinch=fire already routes through UShiftXRHandInteractor, this adds the 6DoF-controller path.
	void AddInputMappingContext();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> ShootRightAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ShootLeftAction;

	/** Claim the in-run weapon-roller offer (controller grip / hand grab). */
	UPROPERTY()
	TObjectPtr<UInputAction> ClaimWeaponAction;

	bool bFlatMode = false;
	bool bIsDead = false;
	float MoveForwardAxis = 0.f;
	float MoveRightAxis = 0.f;
};
