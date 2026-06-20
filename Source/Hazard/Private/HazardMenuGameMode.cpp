#include "HazardMenuGameMode.h"
#include "HazardMenuController.h"
#include "HazardMenuHUD.h"

AHazardMenuGameMode::AHazardMenuGameMode()
{
	PlayerControllerClass = AHazardMenuController::StaticClass();
	HUDClass = AHazardMenuHUD::StaticClass();
}
