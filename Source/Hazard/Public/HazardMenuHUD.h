#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HazardMenuHUD.generated.h"

/** Canvas main-menu / settings screen (greybox; UMG art is a later pass). */
UCLASS()
class HAZARD_API AHazardMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
