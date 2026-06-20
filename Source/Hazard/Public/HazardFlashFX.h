#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HazardFlashFX.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Cheap, pooled, local-only cosmetic flash (emissive sphere that scales up + fades out).
 * Mobile-XR friendly: low tris, unlit, no shadow, no collision, no replication. Reused via
 * UHazardFXSubsystem; auto-deactivates when its short lifetime elapses.
 */
UCLASS()
class HAZARD_API AHazardFlashFX : public AActor
{
	GENERATED_BODY()

public:
	AHazardFlashFX();

	virtual void Tick(float DeltaSeconds) override;

	void Play(const FVector& Location, const FLinearColor& Color, float Size, float Duration);
	void Deactivate();
	bool IsActiveFX() const { return bActive; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FX")
	UStaticMeshComponent* Mesh;

private:
	bool bActive = false;
	float Elapsed = 0.f;
	float Duration = 0.15f;
	float MaxSize = 30.f;
	FLinearColor BaseColor = FLinearColor::White;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;
};
