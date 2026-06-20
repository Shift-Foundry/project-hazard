#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

SHIFTXRCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogShiftXR, Log, All);

class FShiftXRCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
