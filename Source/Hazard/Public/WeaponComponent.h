#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "HazardTypes.h"
#include "WeaponComponent.generated.h"

class AWeaponBase;
class UShiftXRHandInteractor;
class UMotionControllerComponent;

/**
 * Per-hand weapon manager on the player pawn. Spawns/owns an AWeaponBase, routes triggers from
 * hand gestures (VR), input (desktop), or auto-fire (debug), and resolves the aim ray
 * (hand aim when tracked, else camera forward).
 */
UCLASS(ClassGroup = (Hazard), meta = (BlueprintSpawnableComponent))
class HAZARD_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EControllerHand Hand = EControllerHand::Right;

	/** Weapon to spawn. If null, a fallback AWeaponBase with FallbackMode is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<AWeaponBase> WeaponClass;

	/** Mode for the fallback weapon when WeaponClass is unset (Right=ranged, Left=melee). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EWeaponMode FallbackMode = EWeaponMode::RangedHitscan;

	/** Server: re-equip the held weapon from a data-table definition (rarity weapon swap). */
	void EquipWeaponDef(const FWeaponDef& Def);

	/** Fire/melee with the current weapon, resolving aim automatically. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void TriggerPrimary();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AWeaponBase* GetWeapon() const { return Weapon; }

	void SetHandInteractor(UShiftXRHandInteractor* In) { HandRef = In; }
	void SetAttachTarget(USceneComponent* In) { AttachTarget = In; }
	void SetAimController(UMotionControllerComponent* In) { AimController = In; }

protected:
	UFUNCTION()
	void HandleGestureStarted(EShiftGesture Gesture);

private:
	UPROPERTY(Replicated)
	TObjectPtr<AWeaponBase> Weapon = nullptr;

	UPROPERTY()
	TObjectPtr<UShiftXRHandInteractor> HandRef = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> AttachTarget = nullptr;

	UPROPERTY()
	TObjectPtr<UMotionControllerComponent> AimController = nullptr;

	float AutoFireTimer = 0.f;

	bool GetAim(FVector& OutOrigin, FVector& OutDir) const;
	void AutoFireTick(float DeltaTime);
};
