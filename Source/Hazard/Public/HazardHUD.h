#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HazardHUD.generated.h"

/**
 * Minimal Canvas HUD: health, ammo, wave, and score. (A world-space UMG HUD is a later
 * polish pass; Canvas is robust to author from code and renders in PIE and on-device.)
 */
UCLASS()
class HAZARD_API AHazardHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
