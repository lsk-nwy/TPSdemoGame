#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Flow/CandyFightFlow.h"
#include "CandyFight/Interaction/CandyFightSearchComponent.h"
#include "CandyFight/Interaction/CandyFightEscapeComponent.h"
#include "CandyFight/Inventory/CandyFightInventoryComponent.h"
#include "CandyFight/Inventory/CandyFightItem.h"
#include "CandyFight/Inventory/CandyFightItemPickup.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/CandyFightCameraConfig.h"
#include "CandyFight/CandyFightFireCameraComponent.h"
#include "CandyFight/UI/CandyFightCrosshairWidget.h"
#include "CandyFight/CandyFightMovementConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "TPSdemo.h"
#include "SceneView.h"
#include "Widgets/SViewport.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ACandyFightCharacter::ACandyFightCharacter()
{
	Search = CreateDefaultSubobject<UCandyFightSearchComponent>(TEXT("Search"));
	Escape = CreateDefaultSubobject<UCandyFightEscapeComponent>(TEXT("Escape"));
	Inventory = CreateDefaultSubobject<UCandyFightInventoryComponent>(TEXT("Inventory"));
	Combat = CreateDefaultSubobject<UCandyFightCombatComponent>(TEXT("Combat"));
	FireCameraFeedback = CreateDefaultSubobject<UCandyFightFireCameraComponent>(TEXT("FireCameraFeedback"));
	// 移动方向与瞄准朝向独立，朝向由最终镜头下的准星决定。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->MinAnalogWalkSpeed = 0.0f;
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 三个网格只负责显示，碰撞由 ACharacter 自带的胶囊组件负责。
	CapsuleBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapsuleBody"));
	CapsuleTop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapsuleTop"));
	CapsuleBottom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapsuleBottom"));
	for (UStaticMeshComponent* Part : {CapsuleBody.Get(), CapsuleTop.Get(), CapsuleBottom.Get()})
	{
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
	}

	WeaponHoldPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponHoldPoint"));
	WeaponHoldPoint->SetupAttachment(GetCapsuleComponent());
	WeaponHoldPoint->SetRelativeLocation(FVector(30., 48., 20.));

	// 弹簧臂使用世界旋转，避免角色转身带动镜头。
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bUseCameraLagSubstepping = true; // 将较长帧拆分计算，改善跟随平滑。
	CameraBoom->bDoCollisionTest = false;

	// 将透视摄像机挂在弹簧臂末端。
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->ProjectionMode = ECameraProjectionMode::Perspective;
}

// 检查配置、输入和显示网格是否齐全且有效。
bool ACandyFightCharacter::ValidateConfiguration(FString& OutError) const
{
	if (!MovementConfig || !CameraConfig)
	{
		OutError = TEXT("Assign both MovementConfig and CameraConfig DataAssets in the CandyFight character blueprint.");
		return false;
	}
	if (!MovementConfig->ValidateConfiguration(OutError) || !CameraConfig->ValidateConfiguration(OutError))
	{
		return false;
	}
	if (!MoveInputAction || MoveInputAction->ValueType != EInputActionValueType::Axis2D ||
		!MovementMappingContext || !MovementMappingContext->HasMappingForInputAction(MoveInputAction))
	{
		OutError = TEXT("Assign an Axis2D move action and a mapping context containing that action.");
		return false;
	}
	if (!CapsuleBody->GetStaticMesh() || !CapsuleTop->GetStaticMesh() || !CapsuleBottom->GetStaticMesh())
	{
		OutError = TEXT("Assign Engine BasicShapes Cylinder to CapsuleBody and Sphere to CapsuleTop/CapsuleBottom.");
		return false;
	}
	return true;
}

bool ACandyFightCharacter::ApplyConfiguration()
{
	FString Error;
	bConfigurationReady = ValidateConfiguration(Error);
	// 配置错误时记录原因，游戏中同时禁用移动。
	if (!bConfigurationReady)
	{
		if (HasActorBegunPlay())
		{
			UE_LOG(LogTPSdemo, Error, TEXT("CandyFight %s: %s"), *GetName(), *Error);
			GetCharacterMovement()->DisableMovement();
		}
		else
		{
			UE_LOG(LogTPSdemo, Warning, TEXT("CandyFight %s: %s"), *GetName(), *Error);
		}
		return false;
	}

	// 移动组件负责加速、制动、重力和地面碰撞。
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = MovementConfig->MaxWalkSpeed;
	Movement->MaxAcceleration = MovementConfig->MaxAcceleration;
	Movement->BrakingDecelerationWalking = MovementConfig->BrakingDeceleration;
	Movement->GroundFriction = MovementConfig->GroundFriction;
	Movement->RotationRate = FRotator(0.0f, MovementConfig->TurnRate, 0.0f);
	Movement->MinAnalogWalkSpeed = 0.0f;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->SetCapsuleSize(MovementConfig->CapsuleRadius, MovementConfig->CapsuleHalfHeight);

	// 引擎基础网格半径为 50 cm、圆柱高为 100 cm，按碰撞胶囊尺寸缩放。
	const float RadiusScale = MovementConfig->CapsuleRadius / 50.0f;
	const float CylinderHalfHeight = MovementConfig->CapsuleHalfHeight - MovementConfig->CapsuleRadius;
	CapsuleBody->SetRelativeScale3D(FVector(RadiusScale, RadiusScale, CylinderHalfHeight / 50.0f));
	CapsuleBody->SetVisibility(CylinderHalfHeight > 0.0f);
	CapsuleTop->SetRelativeScale3D(FVector(RadiusScale));
	CapsuleBottom->SetRelativeScale3D(FVector(RadiusScale));
	CapsuleTop->SetRelativeLocation(FVector(0.0f, 0.0f, CylinderHalfHeight));
	CapsuleBottom->SetRelativeLocation(FVector(0.0f, 0.0f, -CylinderHalfHeight));

	// 读取镜头配置；向下俯视对应 UE 中的负 Pitch。
	CameraBoom->TargetArmLength = CameraConfig->Distance;
	CameraBoom->SetWorldRotation(FRotator(-CameraConfig->PitchDegrees, CameraConfig->YawDegrees, 0.0f));
	CameraBoom->bEnableCameraLag = CameraConfig->FollowLagSpeed > 0.0f;
	CameraBoom->CameraLagSpeed = CameraConfig->FollowLagSpeed;
	CameraBoom->CameraLagMaxDistance = CameraConfig->MaxLagDistance;
	CameraBoom->bDoCollisionTest = CameraConfig->bEnableCollisionTest;
	FollowCamera->SetFieldOfView(CameraConfig->FieldOfView);
	UpdateGamepadDeadZone();
	return true;
}

// 编辑器重建角色时更新预览。
void ACandyFightCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyConfiguration();
}

// 开始游戏时读取最新配置。
void ACandyFightCharacter::BeginPlay()
{
	// Initialize before Blueprint BeginPlay or component-created HUDs can read health.
	const float PreviousHealth = Health;
	MaxHealth = FMath::IsFinite(MaxHealth) && MaxHealth > 0.f ? MaxHealth : 100.f;
	Health = MaxHealth;
	if (Health != PreviousHealth) OnHealthChanged.Broadcast(Health, MaxHealth);
	Super::BeginPlay();
	ApplyConfiguration();
}

float ACandyFightCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* InstigatorController, AActor* DamageCauser)
{
	if (IsActorBeingDestroyed() || !CanBeDamaged() || !IsValid(InstigatorController) ||
		!IsValid(Cast<ACandyFightEnemyCharacter>(InstigatorController->GetPawn())) ||
		!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f || Health <= 0.f) return 0.f;

	// DamageAmount is this hit's delta; projectile CumulativeDamage is only metadata.
	const float PreviousHealth = Health;
	Health = FMath::Max(0.f, Health - FMath::Min(DamageAmount, Health));
	const float Applied = PreviousHealth - Health;
	if (Applied <= 0.f) return 0.f;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnPlayerDamaged.Broadcast(Applied);
	if (!IsActorBeingDestroyed()) Super::TakeDamage(Applied, DamageEvent, InstigatorController, DamageCauser);
	if (Health <= 0.f) if (auto* Flow = Cast<UCandyFightGameInstance>(GetGameInstance())) Flow->FinishBattle(ECandyFightRunOutcome::Defeated);
	return Applied;
}

float ACandyFightCharacter::Heal(float Amount)
{
	if (IsActorBeingDestroyed() || !FMath::IsFinite(Amount) || Amount <= 0.f) return 0.f;
	const float PreviousHealth = Health;
	Health += FMath::Min(Amount, FMath::Max(0.f, MaxHealth - Health));
	const float Restored = Health - PreviousHealth;
	if (Restored > 0.f) OnHealthChanged.Broadcast(Health, MaxHealth);
	return Restored;
}

// 本地玩家接管角色时重建输入映射，先清理旧映射避免重复注册。
void ACandyFightCharacter::PawnClientRestart()
{
	ResetAimAssist();
	FireCameraFeedback->ClearFeedback();
	Super::PawnClientRestart();
	RemoveInputMapping();
	if (!ApplyConfiguration())
	{
		return;
	}
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetLocalPlayer())
	{
		return;
	}
	// Capture relative mouse movement; the virtual crosshair never reads the OS cursor position.
	PlayerController->bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	UGameViewportClient* ViewportClient = PlayerController->GetLocalPlayer()->ViewportClient;
	PlayerController->SetInputMode(InputMode);
	if (ViewportClient)
	{
		ViewportClient->SetMouseLockMode(EMouseLockMode::LockAlways);
		ViewportClient->SetHideCursorDuringCapture(true);
	}
	bCrosshairPositionInitialized = false;
	CrosshairSnapState.Reset();
	AimFramingState.Reset();
	CurrentAimCameraOffset = FVector::ZeroVector;
	CreateCrosshairWidget(PlayerController);
	Combat->InitializeLocalPlayer();
	Search->InitializeLocalPlayer();
	Inventory->InitializeLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (Subsystem)
	{
		// 修改玩家专属副本的死区，避免改动共享输入资产。
		RuntimeMappingContext = DuplicateObject<UInputMappingContext>(MovementMappingContext, this);
		UpdateGamepadDeadZone();
		Subsystem->AddMappingContext(RuntimeMappingContext, 0);
		RegisteredInputSubsystem = Subsystem;
	}
}

void ACandyFightCharacter::UnPossessed()
{
	ResetAimAssist();
	AimFramingState.Reset();
	Escape->CancelEscape();
	Search->CancelSearch();
	Inventory->CloseInventory();
	Inventory->ReleaseLocalPlayer();
	FireCameraFeedback->ClearFeedback();
	Super::UnPossessed();
}

void ACandyFightCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	bHasValidAim = false;
	ON_SCOPE_EXIT { if (!bHasValidAim) ResetAimAssist(); };
	CurrentAimCameraOffset = FVector::ZeroVector;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!bConfigurationReady || !IsLocallyControlled() || !LocalPlayer ||
		!LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
	{
		return;
	}
	// Keep mouse capture confined to the game view after focus or viewport size changes.
	if (LocalPlayer->ViewportClient->Viewport->HasFocus())
	{
		const TSharedPtr<SViewport> ViewportWidget = LocalPlayer->ViewportClient->GetGameViewportWidget();
		if (ViewportWidget.IsValid())
		{
			LocalPlayer->GetSlateOperations().LockMouseToWidget(ViewportWidget.ToSharedRef());
		}
	}

	// 复用当前玩家的视口范围和 FOV 规则，但使用本帧镜头，而非上一帧缓存。
	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		return;
	}
	FMinimalViewInfo ProjectionView = OutResult;
	FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(ProjectionView,
		LocalPlayer->AspectRatioAxisConstraint, ProjectionData.GetConstrainedViewRect(), ProjectionData);
	const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
	if (ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
	{
		return;
	}
	UpdateCrosshairScreenPosition(PlayerController, ViewRect);
	// Equipped weapons use the actual muzzle; the editable hold point is the unequipped fallback.
	const double AimPlaneZ = Combat->GetAimPlaneZ(WeaponHoldPoint->GetComponentLocation().Z);
	ProjectionData.ViewOrigin = OutResult.Location;
	ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(OutResult.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FVector2D Target = CandyFightAimFraming::Target(RawAimScreenPosition, ViewRect,
		FVector2D(CameraConfig->AimScreenEdgeMarginX, CameraConfig->AimScreenEdgeMarginY));
	const FVector2D Framing = AimFramingState.Update(Target, CameraConfig->bEnableAimScreenOffset,
		CameraConfig->AimOffsetInterpSpeed, DeltaTime, GFrameCounter,
		FVector2D(CameraConfig->AimScreenEdgeMarginX, CameraConfig->AimScreenEdgeMarginY));
	if (CameraConfig->bEnableAimScreenOffset)
	{
		// Solve from the unshifted camera every time, independent of player motion and prior output.
		CandyFightAimFraming::SolveOffset(ProjectionData, Framing, GetActorLocation(), CurrentAimCameraOffset);
	}
	OutResult.Location += CurrentAimCameraOffset;
	ProjectionData.ViewOrigin = OutResult.Location;

	// Mouse input frames the camera; assistance corrects combat aim independently of crosshair interpolation.
	if (CandyFightAimFraming::IntersectPlane(ProjectionData, RawAimScreenPosition, AimPlaneZ, AimWorldLocation))
	{
		bHasValidAim = true;
		LastAimUpdateFrame = GFrameCounter;
		if (CrosshairWidget) CrosshairWidget->RecordAimProjection(ProjectionData.ComputeViewProjectionMatrix(), GFrameCounter);
		UpdateCombatAim(ProjectionData, GFrameCounter, DeltaTime);
		SyncCombatAimRotation();
	}
	// Mark the logical view before the camera manager finishes caching it. Visual
	// feedback runs later, after combat, and is never used for aiming or framing.
	FireCameraFeedback->RecordLogicalCamera(GFrameCounter, OutResult.Rotation);
}

void ACandyFightCharacter::UpdateCrosshairScreenPosition(APlayerController* PlayerController, const FIntRect& ViewRect)
{
	if (!bCrosshairPositionInitialized)
	{
		RawAimScreenPosition = CandyFightAimFraming::ToPixels(FVector2D(0.5, 0.5), ViewRect);
		bCrosshairPositionInitialized = true;
	}
	FVector2D MouseDelta = FVector2D::ZeroVector;
	if (CrosshairSnapState.LastInputFrame != GFrameCounter && !Search->bSearching &&
        PlayerController->GetLocalPlayer()->ViewportClient->Viewport->HasFocus())
	{
		PlayerController->GetInputMouseDelta(MouseDelta.X, MouseDelta.Y);
        MouseDelta = !MouseDelta.ContainsNaN() && FMath::IsFinite(CrosshairMouseSensitivity)
            ? FVector2D(MouseDelta.X, -MouseDelta.Y) * FMath::Max(0.f, CrosshairMouseSensitivity) : FVector2D::ZeroVector;
	}
    CrosshairSnapState.UpdateInput(RawAimScreenPosition, MouseDelta, ViewRect, GFrameCounter);
    RawAimScreenPosition = CrosshairSnapState.RawPosition;
    AimScreenPosition = CrosshairSnapState.DisplayPosition;
}

void ACandyFightCharacter::UpdateGamepadDeadZone()
{
	if (!RuntimeMappingContext || !MovementConfig)
	{
		return;
	}
	for (int32 Index = 0; Index < RuntimeMappingContext->GetMappings().Num(); ++Index)
	{
		FEnhancedActionKeyMapping& Mapping = RuntimeMappingContext->GetMapping(Index);
		if (Mapping.Action == MoveInputAction && Mapping.Key == EKeys::Gamepad_Left2D)
		{
			// 替换旧死区；径向死区按摇杆整体幅度过滤轻微漂移。
			Mapping.Modifiers.RemoveAll([](const UInputModifier* Modifier) { return Modifier && Modifier->IsA<UInputModifierDeadZone>(); });
			UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(RuntimeMappingContext);
			DeadZone->Type = EDeadZoneType::Radial;
			DeadZone->LowerThreshold = MovementConfig->GamepadDeadZone;
			DeadZone->UpperThreshold = 1.0f;
			Mapping.Modifiers.Insert(DeadZone, 0);
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = RegisteredInputSubsystem.Get())
	{
		// 通知输入系统重新读取修改后的映射。
		Subsystem->RequestRebuildControlMappings(FModifyContextOptions(), EInputMappingRebuildType::Rebuild);
	}
}

// 只移除当前角色注册的映射，并清空运行时引用。
void ACandyFightCharacter::RemoveInputMapping()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = RegisteredInputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(RuntimeMappingContext);
	}
	RegisteredInputSubsystem.Reset();
	RuntimeMappingContext = nullptr;
}

void ACandyFightCharacter::CrosshairSpread(float NewSpread)
{
	if (CrosshairWidget)
	{
		CrosshairWidget->SetSpread(NewSpread);
	}
}

void ACandyFightCharacter::WeaponTestEquip(int32 WeaponNumber)
{
#if !UE_BUILD_SHIPPING
	const bool bSuccess = Combat && Combat->EquipTestWeapon(WeaponNumber);
	const FString Message = bSuccess ? FString::Printf(TEXT("WeaponTestEquip %d"), WeaponNumber)
		: TEXT("Usage: WeaponTestEquip 1 (semi-auto), 2 (automatic), 3 (shotgun). Check weapon configuration.");
	UE_LOG(LogTPSdemo, Display, TEXT("%s"), *Message);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ClientMessage(Message);
#endif
}

void ACandyFightCharacter::ElementTestSwitch(int32 ElementIndex)
{
#if !UE_BUILD_SHIPPING
	ECandyFightElement Element = ECandyFightElement::None;
	switch (ElementIndex)
	{
	case 1: Element = ECandyFightElement::Fire; break;
	case 2: Element = ECandyFightElement::Water; break;
	case 3: Element = ECandyFightElement::Ice; break;
	default: Element = CandyFightElements::GetNextElement(Combat ? Combat->GetElement() : ECandyFightElement::Fire); break;
	}
	const bool bSuccess = Combat && Combat->SetElement(Element);
	const FString Message = bSuccess
		? FString::Printf(TEXT("ElementTestSwitch %s"), CandyFightElements::GetElementId(Element))
		: TEXT("Usage: ElementTestSwitch 1 (fire), 2 (water), 3 (ice); other values cycle.");
	UE_LOG(LogTPSdemo, Display, TEXT("%s"), *Message);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ClientMessage(Message);
#endif
}

void ACandyFightCharacter::InventoryTestAdd(const FString& ItemId)
{
#if !UE_BUILD_SHIPPING
	if (!Inventory) return;
	const FName Id(*ItemId);
	const int32 Slot = Inventory->AddItem(Id);
	const FString Message = Slot != INDEX_NONE
		? FString::Printf(TEXT("InventoryTestAdd %s -> slot %d"), *ItemId, Slot)
		: FString::Printf(TEXT("Cannot add '%s'. Use MintCandy, EnergyDrink, GoldTicket, EmergencyKit or CaramelChunk; inventory may be full."), *ItemId);
	UE_LOG(LogTPSdemo, Display, TEXT("%s"), *Message);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ClientMessage(Message);
#endif
}

void ACandyFightCharacter::InventoryTestSpawn(const FString& ItemId)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!Inventory || !World) return;
	FVector Target = GetActorLocation() + GetActorForwardVector() * 150.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightInventoryTestSpawn), false, this);
	FVector Location = Target;
	if (World->LineTraceSingleByChannel(Hit, Target + FVector(0.f, 0.f, 120.f),
		Target - FVector(0.f, 0.f, 320.f), ECC_Visibility, Params))
	{
		Location = Hit.ImpactPoint + FVector(0.f, 0.f, 35.f);
	}
	auto* Pickup = ACandyFightItemPickup::SpawnItem(World, FName(*ItemId), Location,
		Inventory->PickupClass, this, .3f);
	const FString Message = Pickup ? FString::Printf(TEXT("InventoryTestSpawn %s"), *ItemId)
		: FString::Printf(TEXT("Cannot spawn '%s'."), *ItemId);
	UE_LOG(LogTPSdemo, Display, TEXT("%s"), *Message);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ClientMessage(Message);
#endif
}

void ACandyFightCharacter::CreateCrosshairWidget(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	RemoveCrosshairWidget();
	const TSubclassOf<UCandyFightCrosshairWidget> WidgetClass = CrosshairWidgetClass
		? CrosshairWidgetClass.Get() : UCandyFightCrosshairWidget::StaticClass();
	CrosshairWidget = CreateWidget<UCandyFightCrosshairWidget>(PlayerController, WidgetClass);
	if (CrosshairWidget)
	{
		// Full stretch with zero margins follows viewport resizing and DPI changes.
		CrosshairWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CrosshairWidget->AddToPlayerScreen(100);
	}
}

void ACandyFightCharacter::RemoveCrosshairWidget()
{
	if (CrosshairWidget)
	{
		CrosshairWidget->RemoveFromParent();
		CrosshairWidget = nullptr;
	}
}

// 退出 Play 或销毁角色时清理输入映射。
void ACandyFightCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveInputMapping();
	RemoveCrosshairWidget();
	if (Inventory)
	{
		Inventory->CloseInventory();
		Inventory->ReleaseLocalPlayer();
	}
	Super::EndPlay(EndPlayReason);
}

void ACandyFightCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput || !MoveInputAction)
	{
		UE_LOG(LogTPSdemo, Error, TEXT("CandyFight requires Enhanced Input and a move action."));
		return;
	}
	// 持续输入时移动，输入结束或取消时清理待处理输入。
	EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &ACandyFightCharacter::Move);
	EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &ACandyFightCharacter::StopMove);
	EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Canceled, this, &ACandyFightCharacter::StopMove);
	Combat->BindInput(EnhancedInput);
	Search->BindInput(EnhancedInput);
	Inventory->BindInput(EnhancedInput);
}

void ACandyFightCharacter::Move(const FInputActionValue& Value)
{
	if (Search->bSearching || !bConfigurationReady || !GetController())
	{
		return;
	}
	const FVector2D Input = Value.Get<FVector2D>();
	if (!FMath::IsFinite(Input.X) || !FMath::IsFinite(Input.Y) || Input.IsNearlyZero())
	{
		return;
	}
	// 分离方向与幅度，让键盘斜向同速，同时保留摇杆轻推的慢速。
	const FVector2D Direction = Input.GetSafeNormal();
	// 按当前镜头的水平朝向，将屏幕方向转换为世界方向。
	const FRotationMatrix CameraYaw(FRotator(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f));
	const FVector WorldDirection = CameraYaw.GetUnitAxis(EAxis::X) * Direction.Y + CameraYaw.GetUnitAxis(EAxis::Y) * Direction.X;
	AddMovementInput(WorldDirection.GetSafeNormal(), FMath::Min(Input.Size(), 1.0)); // 输入幅度最大为 1。
}

void ACandyFightCharacter::StopMove(const FInputActionValue& Value)
{
	ConsumeMovementInputVector(); // 清理待处理输入，实际减速由移动组件完成。
}

#if WITH_EDITOR
// 在编辑器资产校验中报告配置错误。
EDataValidationResult ACandyFightCharacter::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateConfiguration(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif

USceneComponent* ACandyFightCharacter::GetCombatMount() const { return WeaponHoldPoint; }
bool ACandyFightCharacter::GetCombatAim(FVector& Location, uint64& Frame) const
{
    Location = AimAssistProjection ? CombatAimWorldLocation : AimWorldLocation;
    Frame = LastAimUpdateFrame;
    return bHasValidAim;
}
bool ACandyFightCharacter::IsCombatShotAllowed() const { return !Search || !Search->bSearching; }

void ACandyFightCharacter::ResetAimAssist()
{
    const bool bWasValid = bHasValidAim;
    ReleaseAimAssist();
    bHasValidAim = bWasValid;
    AimAssistConfig.Reset();
    AimAssistProjection.Reset();
}

void ACandyFightCharacter::ReleaseAimAssist()
{
    bAimAssistActive = false;
    AimAssistTarget.Reset();
    AimAssistWorldLocation = FVector::ZeroVector;
    if (CrosshairSnapState.bInitialized)
    {
        CrosshairSnapState.Release();
        RawAimScreenPosition = CrosshairSnapState.RawPosition;
        AimScreenPosition = CrosshairSnapState.DisplayPosition;
    }
    AimAssistDebug = FCandyFightAimAssistDebug();
    RefreshCombatAim();
    if (CrosshairWidget) CrosshairWidget->SetAimAssistDebug(AimAssistDebug);
}

void ACandyFightCharacter::RefreshCombatAim()
{
    if (!AimAssistProjection) { CombatAimWorldLocation = AimWorldLocation; return; }
    const double Height = Combat->GetAimPlaneZ(WeaponHoldPoint->GetComponentLocation().Z);
    bHasValidAim = CandyFightAimFraming::IntersectPlane(*AimAssistProjection, RawAimScreenPosition, Height, AimWorldLocation);
    if (bAimAssistActive && AimAssistTarget.IsValid())
    {
        CombatAimWorldLocation = AimAssistWorldLocation;
        bHasValidAim = bHasValidAim && !CombatAimWorldLocation.ContainsNaN();
    }
    else
    {
        bHasValidAim = CandyFightAimFraming::IntersectPlane(*AimAssistProjection, AimScreenPosition, Height, CombatAimWorldLocation) && bHasValidAim;
    }
    if (CrosshairWidget)
    {
        CrosshairWidget->SetAimViewRect(AimAssistProjection->GetConstrainedViewRect());
        CrosshairWidget->SetScreenPosition(AimScreenPosition);
    }
}

void ACandyFightCharacter::SyncCombatAimRotation()
{
    auto* PC = Cast<APlayerController>(GetController());
    const FVector Delta = CombatAimWorldLocation - GetActorLocation();
    if (PC && bHasValidAim && (!Search || !Search->bSearching) && Delta.SizeSquared2D() > 1.)
    {
        const FRotator Rotation(0., Delta.Rotation().Yaw, 0.);
        SetActorRotation(Rotation);
        PC->SetControlRotation(Rotation);
    }
}

void ACandyFightCharacter::UpdateCombatAim(const FSceneViewProjectionData& Projection, uint64 Frame, float DeltaTime)
{
    CrosshairSnapState.UpdateInput(RawAimScreenPosition, FVector2D::ZeroVector, Projection.GetConstrainedViewRect(), Frame);
    RawAimScreenPosition = CrosshairSnapState.RawPosition;
    AimScreenPosition = CrosshairSnapState.DisplayPosition;
    AimAssistProjection = MakeShared<FSceneViewProjectionData>(Projection);
    const auto* Config = Combat ? Combat->GetCurrentConfig() : nullptr;
    const auto* PC = Cast<APlayerController>(GetController());
    if (!Config || !Config->bEnableAimAssist || !IsValid(Combat->EquippedWeapon) ||
        !Combat->EquippedWeapon->ValidateMuzzle() || (Search && Search->bSearching) ||
        !PC || (PC->GetLocalPlayer() && !Combat->IsCombatInputAllowed()))
    {
        ResetAimAssist();
        LastAimAssistFrame = Frame;
        AimAssistProjection = MakeShared<FSceneViewProjectionData>(Projection);
        CrosshairSnapState.ReturnToInput(DeltaTime, Frame);
        AimScreenPosition = CrosshairSnapState.DisplayPosition;
        RefreshCombatAim();
        return;
    }
    if (LastAimAssistFrame == Frame)
    {
        RevalidateCombatAim();
        return;
    }
    LastAimAssistFrame = Frame;
    AimAssistConfig = const_cast<UCandyFightWeaponConfig*>(Config);
    CandyFightAimAssist::FCandidate Candidate;
    const bool bDebug = CandyFightAimAssist::IsDebugEnabled();
    AimAssistDebug = FCandyFightAimAssistDebug();
    const bool bFound = CandyFightAimAssist::Resolve(GetWorld(), this, Combat->EquippedWeapon,
        Combat->EquippedWeapon->Muzzle->GetComponentLocation(), Projection, RawAimScreenPosition,
        *Config, AimAssistTarget.Get(), Candidate, bDebug ? &AimAssistDebug : nullptr);
    if (bFound)
    {
        bAimAssistActive = true;
        AimAssistTarget = Candidate.Actor;
        AimAssistWorldLocation = Candidate.Aim;
        CrosshairSnapState.SnapTo(Candidate.ScreenAim, Config->AimAssistSnapSeconds, DeltaTime, Frame);
        AimScreenPosition = CrosshairSnapState.DisplayPosition;
        RefreshCombatAim();
    }
    else
    {
        ReleaseAimAssist();
        CrosshairSnapState.ReturnToInput(DeltaTime, Frame);
        AimScreenPosition = CrosshairSnapState.DisplayPosition;
        RefreshCombatAim();
    }
    if (bDebug)
    {
        const FMatrix Matrix = Projection.ComputeViewProjectionMatrix();
        const FIntRect Rect = Projection.GetConstrainedViewRect();
        AimAssistDebug.bHasDirections = FSceneView::ProjectWorldToScreen(Combat->EquippedWeapon->Muzzle->GetComponentLocation(), Rect, Matrix, AimAssistDebug.Muzzle) &&
            FSceneView::ProjectWorldToScreen(AimWorldLocation, Rect, Matrix, AimAssistDebug.RawAim) &&
            FSceneView::ProjectWorldToScreen(CombatAimWorldLocation, Rect, Matrix, AimAssistDebug.CombatAim);
        AimAssistDebug.RawCursor = RawAimScreenPosition;
        AimAssistDebug.DisplayCursor = AimScreenPosition;
        AimAssistDebug.bHasSnapCenter = bFound;
        AimAssistDebug.SnapCenter = Candidate.ScreenAim;
    }
    if (CrosshairWidget) CrosshairWidget->SetAimAssistDebug(AimAssistDebug);
}

void ACandyFightCharacter::RevalidateCombatAim()
{
    const auto* Config = Combat ? Combat->GetCurrentConfig() : nullptr;
    auto* PC = Cast<APlayerController>(GetController());
    if (!Config || !Config->bEnableAimAssist || !PC || (Search && Search->bSearching) ||
        (PC->GetLocalPlayer() && !Combat->IsCombatInputAllowed()))
    {
        ReleaseAimAssist();
        AimAssistConfig.Reset();
        SyncCombatAimRotation();
        return;
    }
    if (!bAimAssistActive) { RefreshCombatAim(); SyncCombatAimRotation(); return; }
    CandyFightAimAssist::FCandidate Candidate;
    if (Config && Config == AimAssistConfig.Get() && Config->bEnableAimAssist && AimAssistProjection &&
        IsValid(Combat->EquippedWeapon) && Combat->EquippedWeapon->ValidateMuzzle() &&
        CandyFightAimAssist::Evaluate(GetWorld(), this, Combat->EquippedWeapon, AimAssistTarget.Get(),
            Combat->EquippedWeapon->Muzzle->GetComponentLocation(), *AimAssistProjection, RawAimScreenPosition, *Config, Candidate, nullptr, true))
    {
        AimAssistWorldLocation = Candidate.Aim;
        RefreshCombatAim();
        SyncCombatAimRotation();
        return;
    }
    // Keep the frame stamp: a second camera evaluation must not acquire another target this frame.
    ReleaseAimAssist();
    SyncCombatAimRotation();
}
