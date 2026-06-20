#include "ShiftXRCore.h"

DEFINE_LOG_CATEGORY(LogShiftXR);

void FShiftXRCoreModule::StartupModule()
{
	UE_LOG(LogShiftXR, Log, TEXT("ShiftXRCore module started."));
}

void FShiftXRCoreModule::ShutdownModule()
{
	UE_LOG(LogShiftXR, Log, TEXT("ShiftXRCore module shut down."));
}

IMPLEMENT_MODULE(FShiftXRCoreModule, ShiftXRCore);
