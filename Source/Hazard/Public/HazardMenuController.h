#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HazardMenuController.generated.h"

UENUM()
enum class EMenuScreen : uint8 { Main, Settings, Shop };

/** One purchasable lobby-shop row (weapon unlock or class unlock/select). */
struct FHazardShopEntry
{
	FName Row;
	bool bIsClass = false;
	FString Name;
	FString Desc;
	int32 Cost = 0;
	uint8 Rarity = 0;
};

/** Drives the main-menu level: keyboard-navigated screens (Canvas HUD draws them). */
UCLASS()
class HAZARD_API AHazardMenuController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(BlueprintReadOnly, Category = "Hazard")
	EMenuScreen Screen = EMenuScreen::Main;

	UPROPERTY(BlueprintReadOnly, Category = "Hazard")
	int32 GraphicsLevel = 2; // 0..3 (Scalability)

	UPROPERTY(BlueprintReadOnly, Category = "Hazard")
	int32 ShopCursor = 0;

	/** Build the lobby-shop entry list (buyable weapons, then classes). Shared by HUD + controller. */
	static void BuildShopEntries(TArray<FHazardShopEntry>& Out);

protected:
	void OnKey1();
	void OnKey2();
	void OnKey3();
	void OnKey4();
	void OnKey5();
	void OnKey6();
	void OnShopUp();
	void OnShopDown();
	void OnShopConfirm();
	void OnBack();

	void ApplyGraphics();
};
