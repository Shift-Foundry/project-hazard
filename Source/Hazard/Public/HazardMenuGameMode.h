#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HazardMenuGameMode.generated.h"

/** GameMode for L_MainMenu: menu controller + Canvas menu HUD. */
UCLASS()
class HAZARD_API AHazardMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHazardMenuGameMode();
};
