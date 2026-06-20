#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShiftXRTypes.h"
#include "ShiftXRPerfGovernor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShiftPerfTierChanged, EShiftPerfTier, NewTier);

/**
 * Per-world performance governor. Smooths frame time and walks a hysteresis tier ladder,
 * mapping each tier to a dynamic-resolution target and an enemy budget the WaveDirector
 * obeys. Logs every tier change (LogShiftXR). Note: foveation/VRS/SpaceWarp are boot-time
 * (ECVF_ReadOnly) in UE 5.8, so they are configured via ini, not driven here per-frame.
 */
UCLASS()
class SHIFTXRCORE_API UShiftXRPerfGovernor : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableObjectBase / UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Force a specific tier and apply it immediately. */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	void SetTier(EShiftPerfTier NewTier);

	/** Enable/disable automatic tier governing. */
	UFUNCTION(BlueprintCallable, Category = "ShiftXR")
	void SetAutoGovern(bool bEnabled) { bAutoGovern = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	EShiftPerfTier GetTier() const { return CurrentTier; }

	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	float GetSmoothedFrameMs() const { return SmoothedFrameMs; }

	/** Enemy budget for the current tier; the WaveDirector caps concurrent spawns to this. */
	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	int32 GetMaxConcurrentEnemies() const;

	/** VFX density multiplier for the current tier (1=full ... 0=skip). FX respect this to protect frame-rate. */
	UFUNCTION(BlueprintPure, Category = "ShiftXR")
	float GetVFXScale() const;

	UPROPERTY(BlueprintAssignable, Category = "ShiftXR")
	FOnShiftPerfTierChanged OnTierChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShiftXR")
	bool bAutoGovern = true;

	/** Target frame budget (ms). 90 fps locked target is ~11.1 ms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShiftXR", meta = (ClampMin = "4.0"))
	float TargetFrameMs = 11.1f;

private:
	EShiftPerfTier CurrentTier = EShiftPerfTier::High;
	float SmoothedFrameMs = 11.1f;
	float TimeSinceTierChange = 0.f;

	void EvaluateAutoTier();
	void ApplyTier(EShiftPerfTier Tier);
};
