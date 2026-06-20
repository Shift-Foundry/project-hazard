#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HazardFXSubsystem.generated.h"

class AHazardFlashFX;

/**
 * Pools AHazardFlashFX and gates spawns by the PerfGovernor's VFX scale, so cosmetic FX never
 * cost the frame-rate budget on the glasses. Call SpawnFlash from cosmetic (multicast) paths.
 */
UCLASS()
class HAZARD_API UHazardFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Hazard|FX")
	void SpawnFlash(const FVector& Location, const FLinearColor& Color, float Size);

private:
	UPROPERTY()
	TArray<TObjectPtr<AHazardFlashFX>> Pool;

	static constexpr int32 MaxPool = 48;
};
