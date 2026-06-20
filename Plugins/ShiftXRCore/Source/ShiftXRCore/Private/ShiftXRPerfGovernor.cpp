#include "ShiftXRPerfGovernor.h"
#include "ShiftXRCore.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Stats/Stats.h"

void UShiftXRPerfGovernor::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentTier = EShiftPerfTier::High;
	SmoothedFrameMs = TargetFrameMs;
	TimeSinceTierChange = 0.f;
	ApplyTier(CurrentTier);
	UE_LOG(LogShiftXR, Display, TEXT("[PerfGovernor] Initialized. Target %.1f ms, starting tier=%s, maxEnemies=%d"),
		TargetFrameMs, *UEnum::GetValueAsString(CurrentTier), GetMaxConcurrentEnemies());
}

void UShiftXRPerfGovernor::Deinitialize()
{
	Super::Deinitialize();
}

bool UShiftXRPerfGovernor::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UShiftXRPerfGovernor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DeltaTime <= 0.f)
	{
		return;
	}

	const float FrameMs = DeltaTime * 1000.f;
	SmoothedFrameMs = FMath::FInterpTo(SmoothedFrameMs, FrameMs, DeltaTime, 4.f);
	TimeSinceTierChange += DeltaTime;

	if (bAutoGovern)
	{
		EvaluateAutoTier();
	}
}

TStatId UShiftXRPerfGovernor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShiftXRPerfGovernor, STATGROUP_Tickables);
}

void UShiftXRPerfGovernor::EvaluateAutoTier()
{
	const float UpThreshold = TargetFrameMs * 1.15f;   // over budget -> drop a tier
	const float DownThreshold = TargetFrameMs * 0.80f; // comfortable -> raise a tier
	const float MinDwellSeconds = 2.0f;

	if (TimeSinceTierChange < MinDwellSeconds)
	{
		return;
	}

	const int32 TierIdx = static_cast<int32>(CurrentTier);
	if (SmoothedFrameMs > UpThreshold && TierIdx < static_cast<int32>(EShiftPerfTier::Emergency))
	{
		SetTier(static_cast<EShiftPerfTier>(TierIdx + 1));
	}
	else if (SmoothedFrameMs < DownThreshold && TierIdx > static_cast<int32>(EShiftPerfTier::Ultra))
	{
		SetTier(static_cast<EShiftPerfTier>(TierIdx - 1));
	}
}

void UShiftXRPerfGovernor::SetTier(EShiftPerfTier NewTier)
{
	if (NewTier == CurrentTier)
	{
		return;
	}

	const EShiftPerfTier OldTier = CurrentTier;
	CurrentTier = NewTier;
	TimeSinceTierChange = 0.f;
	ApplyTier(NewTier);

	UE_LOG(LogShiftXR, Display, TEXT("[PerfGovernor] Tier %s -> %s (%.1f ms smoothed, maxEnemies=%d)"),
		*UEnum::GetValueAsString(OldTier), *UEnum::GetValueAsString(NewTier), SmoothedFrameMs, GetMaxConcurrentEnemies());

	OnTierChanged.Broadcast(NewTier);
}

void UShiftXRPerfGovernor::ApplyTier(EShiftPerfTier Tier)
{
	// Two levers: vr.PixelDensity actually resizes the stereo eye render targets on an XR device
	// (the correct GPU-fillrate defense), and r.ScreenPercentage covers flat/PC mode where
	// vr.PixelDensity is a no-op.
	float ScreenPct = 100.f;
	float PixelDensity = 1.0f;
	switch (Tier)
	{
	case EShiftPerfTier::Ultra:     ScreenPct = 100.f; PixelDensity = 1.00f; break;
	case EShiftPerfTier::High:      ScreenPct = 90.f;  PixelDensity = 0.90f; break;
	case EShiftPerfTier::Medium:    ScreenPct = 80.f;  PixelDensity = 0.80f; break;
	case EShiftPerfTier::Low:       ScreenPct = 70.f;  PixelDensity = 0.72f; break;
	case EShiftPerfTier::Emergency: ScreenPct = 60.f;  PixelDensity = 0.65f; break;
	default: break;
	}

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.PixelDensity")))
	{
		CVar->Set(*FString::SanitizeFloat(PixelDensity), ECVF_SetByGameSetting);
	}
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		CVar->Set(*FString::SanitizeFloat(ScreenPct), ECVF_SetByGameSetting);
	}
}

int32 UShiftXRPerfGovernor::GetMaxConcurrentEnemies() const
{
	switch (CurrentTier)
	{
	case EShiftPerfTier::Ultra:     return 40;
	case EShiftPerfTier::High:      return 30;
	case EShiftPerfTier::Medium:    return 22;
	case EShiftPerfTier::Low:       return 16;
	case EShiftPerfTier::Emergency: return 10;
	default:                        return 20;
	}
}

float UShiftXRPerfGovernor::GetVFXScale() const
{
	switch (CurrentTier)
	{
	case EShiftPerfTier::Ultra:     return 1.0f;
	case EShiftPerfTier::High:      return 1.0f;
	case EShiftPerfTier::Medium:    return 0.7f;
	case EShiftPerfTier::Low:       return 0.4f;
	case EShiftPerfTier::Emergency: return 0.0f; // drop cosmetic FX to hold frame-rate
	default:                        return 1.0f;
	}
}
