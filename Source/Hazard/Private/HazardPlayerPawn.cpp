#include "HazardPlayerPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "HealthComponent.h"
#include "WeaponComponent.h"
#include "ColocationComponent.h"
#include "RunManager.h"
#include "ShiftXRHandInteractor.h"
#include "HazardPlayerController.h"
#include "WeaponLibrary.h"
#include "ClassLibrary.h"
#include "HazardGameInstance.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "HeadMountedDisplayTypes.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AHazardPlayerPawn::AHazardPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
	Camera->bLockToHmd = true;

	LeftHandRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHandRoot"));
	LeftHandRoot->SetupAttachment(VROrigin);

	RightHandRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RightHandRoot"));
	RightHandRoot->SetupAttachment(VROrigin);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> HandMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	LeftHandMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandMarker"));
	LeftHandMarker->SetupAttachment(LeftHandRoot);
	LeftHandMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftHandMarker->SetRelativeScale3D(FVector(0.08f));

	RightHandMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandMarker"));
	RightHandMarker->SetupAttachment(RightHandRoot);
	RightHandMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandMarker->SetRelativeScale3D(FVector(0.08f));

	if (HandMesh.Succeeded())
	{
		LeftHandMarker->SetStaticMesh(HandMesh.Object);
		RightHandMarker->SetStaticMesh(HandMesh.Object);
	}

	LeftHand = CreateDefaultSubobject<UShiftXRHandInteractor>(TEXT("LeftHand"));
	LeftHand->Hand = EControllerHand::Left;

	RightHand = CreateDefaultSubobject<UShiftXRHandInteractor>(TEXT("RightHand"));
	RightHand->Hand = EControllerHand::Right;

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));

	RightWeapon = CreateDefaultSubobject<UWeaponComponent>(TEXT("RightWeapon"));
	RightWeapon->Hand = EControllerHand::Right;
	RightWeapon->FallbackMode = EWeaponMode::RangedHitscan;

	LeftWeapon = CreateDefaultSubobject<UWeaponComponent>(TEXT("LeftWeapon"));
	LeftWeapon->Hand = EControllerHand::Left;
	LeftWeapon->FallbackMode = EWeaponMode::Melee;

	Colocation = CreateDefaultSubobject<UColocationComponent>(TEXT("Colocation"));

	// OpenXR controller aim-pose sources (drive aim when a 6DoF controller is tracked).
	RightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightAim"));
	RightAim->SetupAttachment(VROrigin);
	RightAim->SetTrackingMotionSource(FName("RightAim"));
	LeftAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftAim"));
	LeftAim->SetupAttachment(VROrigin);
	LeftAim->SetTrackingMotionSource(FName("LeftAim"));

	// Enhanced Input assets (XR 6DoF-controller path). Everything is null-guarded, so a missing
	// asset degrades to "controller doesn't fire" rather than crashing. Hand pinch=fire still
	// works via the gesture path (the primary input on a hands/eyes device like Galaxy XR).
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC(TEXT("/Game/XRFramework/Input/IMC_Default.IMC_Default"));
	if (IMC.Succeeded()) { GameplayMappingContext = IMC.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> SR(TEXT("/Game/XRFramework/Input/Actions/IA_Shoot_Right.IA_Shoot_Right"));
	if (SR.Succeeded()) { ShootRightAction = SR.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> SL(TEXT("/Game/XRFramework/Input/Actions/IA_Shoot_Left.IA_Shoot_Left"));
	if (SL.Succeeded()) { ShootLeftAction = SL.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> Claim(TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Right_Pressed.IA_Grab_Right_Pressed"));
	if (Claim.Succeeded()) { ClaimWeaponAction = Claim.Object; }
}

void AHazardPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Flat mode = no XR runtime (desktop testing). VR mode = HMD drives the camera.
	bFlatMode = !(GEngine && GEngine->XRSystem.IsValid());
	if (!bFlatMode)
	{
		// Prefer Stage (room-scale, floor-corrected). UE 5.8 has no failure callback or
		// OnTrackingOriginChanged delegate, so log the resulting origin for on-device debugging —
		// floor-anchored content (Base/chest at Z=0) assumes the runtime gives us a floor origin.
		GEngine->XRSystem->SetTrackingOrigin(EHMDTrackingOrigin::Stage);
		UE_LOG(LogTemp, Display, TEXT("[XR] Tracking origin requested=Stage, active=%d."),
			static_cast<int32>(GEngine->XRSystem->GetTrackingOrigin()));
	}
	else if (Camera)
	{
		// Desktop free-look camera at standing eye height (no HMD to drive it).
		Camera->bLockToHmd = false;
		Camera->bUsePawnControlRotation = true;
		Camera->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	}

	if (RightWeapon)
	{
		RightWeapon->SetHandInteractor(RightHand);
		RightWeapon->SetAttachTarget(RightHandRoot);
		RightWeapon->SetAimController(RightAim);
	}
	if (LeftWeapon)
	{
		LeftWeapon->SetHandInteractor(LeftHand);
		LeftWeapon->SetAttachTarget(LeftHandRoot);
		LeftWeapon->SetAimController(LeftAim);
	}

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &AHazardPlayerPawn::HandlePlayerDeath);
	}

	// Server: apply the selected class (slots / starting weapon / health), then seed the arsenal.
	// (RightWeapon was spawned during Super::BeginPlay.)
	if (HasAuthority())
	{
		FName ClassRow = FName("Recruit");
		if (UWorld* W = GetWorld())
		{
			if (UHazardGameInstance* GI = W->GetGameInstance<UHazardGameInstance>())
			{
				ClassRow = GI->GetSelectedClass();
			}
		}
		const FClassDef Cls = UClassLibrary::FindClass(ClassRow);
		MaxWeaponSlots = FMath::Max(1, Cls.WeaponSlots); // set before seeding so slot logic is correct
		if (Health)
		{
			Health->MaxHealth = 100.f + Cls.HealthBonus;
			Health->ResetToFull();
		}

		const FName StartRow = Cls.StartWeaponRow.IsNone() ? FName("Pistol") : Cls.StartWeaponRow;
		const FString StartName = StartRow.ToString();
		FWeaponDef Start;
		bool bFound = false;
		for (const FWeaponDef& D : UWeaponLibrary::GetAllWeapons())
		{
			if (D.RowName == StartRow) { Start = D; bFound = true; break; }
		}
		if (!bFound)
		{
			Start.RowName = FName("Pistol");
			Start.DisplayName = TEXT("Pistol");
			Start.Rarity = EItemRarity::Common;
		}
		EquipToArsenal(Start);
		UE_LOG(LogTemp, Display, TEXT("[Class] %s applied: slots=%d, +HP=%.0f, dmgx=%.2f, start=%s"),
			*Cls.DisplayName, MaxWeaponSlots, Cls.HealthBonus, Cls.DamageMult, *StartName);
	}
}

void AHazardPlayerPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHazardPlayerPawn, ArsenalNames);
	DOREPLIFETIME(AHazardPlayerPawn, ArsenalRarities);
	DOREPLIFETIME(AHazardPlayerPawn, ActiveSlot);
}

void AHazardPlayerPawn::EquipToArsenal(const FWeaponDef& Def)
{
	if (!HasAuthority())
	{
		return;
	}
	const int32 Cap = FMath::Max(1, MaxWeaponSlots);
	if (Arsenal.Num() < Cap)
	{
		Arsenal.Add(Def);
		ActiveSlot = Arsenal.Num() - 1; // auto-switch to the newly picked weapon
	}
	else
	{
		Arsenal[ActiveSlot] = Def; // arsenal full -> replace the active slot
	}
	RefreshArsenalMirror();
	EquipActiveToHand();
	UE_LOG(LogTemp, Display, TEXT("[Arsenal] +%s [%s] -> slot %d (%d/%d used)"), *Def.DisplayName,
		*UWeaponLibrary::RarityName(Def.Rarity), ActiveSlot, Arsenal.Num(), FMath::Max(1, MaxWeaponSlots));
}

void AHazardPlayerPawn::CycleWeapon()
{
	if (!HasAuthority() || Arsenal.Num() <= 1)
	{
		return;
	}
	ActiveSlot = (ActiveSlot + 1) % Arsenal.Num();
	RefreshArsenalMirror();
	EquipActiveToHand();
}

void AHazardPlayerPawn::EquipActiveToHand()
{
	if (RightWeapon && Arsenal.IsValidIndex(ActiveSlot))
	{
		RightWeapon->EquipWeaponDef(Arsenal[ActiveSlot]);
	}
}

void AHazardPlayerPawn::RefreshArsenalMirror()
{
	ArsenalNames.Reset();
	ArsenalRarities.Reset();
	for (const FWeaponDef& D : Arsenal)
	{
		ArsenalNames.Add(D.DisplayName);
		ArsenalRarities.Add(static_cast<uint8>(D.Rarity));
	}
}

void AHazardPlayerPawn::OnCycleWeaponInput()
{
	if (AHazardPlayerController* PC = Cast<AHazardPlayerController>(GetController()))
	{
		PC->ServerCycleWeapon();
	}
}

void AHazardPlayerPawn::OnClaimRolledWeapon()
{
	if (AHazardPlayerController* PC = Cast<AHazardPlayerController>(GetController()))
	{
		PC->ServerClaimRolledWeapon();
	}
}

void AHazardPlayerPawn::HandlePlayerDeath(AActor* Killer)
{
	// Fires on the server (damage path) and on clients (HealthComponent OnRep_Dead) so the
	// hands hide everywhere. Only the server ends the run.
	bIsDead = true;
	if (LeftHandRoot) { LeftHandRoot->SetVisibility(false, true); }
	if (RightHandRoot) { RightHandRoot->SetVisibility(false, true); }
	if (HasAuthority())
	{
		if (UWorld* W = GetWorld())
		{
			if (URunManager* RM = W->GetSubsystem<URunManager>())
			{
				RM->NotifyPlayerDied();
			}
		}
	}
}

void AHazardPlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHand(LeftHand, LeftHandRoot);
	UpdateHand(RightHand, RightHandRoot);

	if (bFlatMode && Controller)
	{
		const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
		const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
		const FVector Delta = (Fwd * MoveForwardAxis + Right * MoveRightAxis) * FlatMoveSpeed * DeltaSeconds;
		if (!Delta.IsNearlyZero())
		{
			AddActorWorldOffset(Delta, true);
		}
	}
}

void AHazardPlayerPawn::UpdateHand(UShiftXRHandInteractor* HandComp, USceneComponent* HandRoot)
{
	if (!HandComp || !HandRoot || bIsDead)
	{
		return; // keep hands hidden after death (Tick would otherwise re-show them)
	}

	FTransform Palm;
	if (HandComp->IsHandTracked() && HandComp->GetPalmTransform(Palm))
	{
		HandRoot->SetWorldLocationAndRotation(Palm.GetLocation(), Palm.GetRotation());
		HandRoot->SetVisibility(true, /*bPropagateToChildren=*/true);
	}
	else
	{
		HandRoot->SetVisibility(false, /*bPropagateToChildren=*/true);
	}
}

void AHazardPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}
	// Desktop testing: LMB = ranged (right), RMB / F = melee (left). VR uses hand gestures.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AHazardPlayerPawn::FireRightPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AHazardPlayerPawn::MeleeLeftPressed);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AHazardPlayerPawn::MeleeLeftPressed);

	// Roguelite: pick one of the 3 offered cards (1/2/3), place a chest (C).
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AHazardPlayerPawn::OnPickCard0);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AHazardPlayerPawn::OnPickCard1);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AHazardPlayerPawn::OnPickCard2);
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AHazardPlayerPawn::OnPlaceChest);

	// Swap between weapon slots: Q or mouse-wheel up.
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AHazardPlayerPawn::OnCycleWeaponInput);
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AHazardPlayerPawn::OnCycleWeaponInput);

	// Claim the in-run weapon-roller offer: G (flat).
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AHazardPlayerPawn::OnClaimRolledWeapon);

	// Desktop flat-mode movement / look (axis mappings in DefaultInput.ini).
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AHazardPlayerPawn::MoveForwardInput);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AHazardPlayerPawn::MoveRightInput);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AHazardPlayerPawn::TurnInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AHazardPlayerPawn::LookUpInput);

	// --- Enhanced Input: XR 6DoF-controller path (additive; legacy mouse/keys above stay for flat mode) ---
	AddInputMappingContext();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Triggered = hold-to-fire (weapon cooldown gates the rate); melee is a single swing per press.
		if (ShootRightAction) { EIC->BindAction(ShootRightAction, ETriggerEvent::Triggered, this, &AHazardPlayerPawn::FireRightPressed); }
		if (ShootLeftAction)  { EIC->BindAction(ShootLeftAction,  ETriggerEvent::Started,   this, &AHazardPlayerPawn::MeleeLeftPressed); }
		if (ClaimWeaponAction){ EIC->BindAction(ClaimWeaponAction, ETriggerEvent::Started,  this, &AHazardPlayerPawn::OnClaimRolledWeapon); }
	}
}

void AHazardPlayerPawn::AddInputMappingContext()
{
	if (!GameplayMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] GameplayMappingContext null (IMC_Default not found) — controller input unbound."));
		return;
	}
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
			{
				Subsys->AddMappingContext(GameplayMappingContext, 0);
				UE_LOG(LogTemp, Display, TEXT("[Input] Enhanced Input mapping context added (IMC_Default)."));
			}
		}
	}
}

void AHazardPlayerPawn::MoveForwardInput(float Value) { MoveForwardAxis = Value; }
void AHazardPlayerPawn::MoveRightInput(float Value) { MoveRightAxis = Value; }
void AHazardPlayerPawn::TurnInput(float Value) { if (bFlatMode && Value != 0.f) { AddControllerYawInput(Value); } }
void AHazardPlayerPawn::LookUpInput(float Value) { if (bFlatMode && Value != 0.f) { AddControllerPitchInput(Value); } }

void AHazardPlayerPawn::OnPickCard0() { PickCardIndex(0); }
void AHazardPlayerPawn::OnPickCard1() { PickCardIndex(1); }
void AHazardPlayerPawn::OnPickCard2() { PickCardIndex(2); }

void AHazardPlayerPawn::PickCardIndex(int32 Index)
{
	if (AHazardPlayerController* PC = Cast<AHazardPlayerController>(GetController()))
	{
		PC->ServerPickCard(Index);
	}
}

void AHazardPlayerPawn::OnPlaceChest()
{
	AHazardPlayerController* PC = Cast<AHazardPlayerController>(GetController());
	if (!PC || !Camera)
	{
		return;
	}
	// Place a chest on the floor a couple of metres ahead of the player.
	const FVector Origin = Camera->GetComponentLocation();
	FVector Fwd = Camera->GetForwardVector();
	Fwd.Z = 0.f;
	FVector Loc = Origin + Fwd.GetSafeNormal() * 250.f;
	Loc.Z = 0.f;
	PC->ServerPlaceChest(Loc);
}

void AHazardPlayerPawn::FireRightPressed()
{
	if (RightWeapon)
	{
		RightWeapon->TriggerPrimary();
	}
}

void AHazardPlayerPawn::MeleeLeftPressed()
{
	if (LeftWeapon)
	{
		LeftWeapon->TriggerPrimary();
	}
}
