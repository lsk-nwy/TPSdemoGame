#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CandyFight/CandyFightAimFraming.h"
#include "CandyFight/CandyFightAimAssist.h"
#include "CandyFight/CandyFightCrosshairSnap.h"
#include "Weapons/CandyFightCombatOwner.h"
#include "CandyFightCharacter.generated.h"

class UCameraComponent;
class UCandyFightCameraConfig;
class UCandyFightCrosshairWidget;
class UCandyFightMovementConfig;
class UCandyFightCombatComponent;
class UCandyFightFireCameraComponent;
class UCandyFightSearchComponent;
class UCandyFightEscapeComponent;
class UCandyFightInventoryComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
class USceneComponent;
struct FInputActionValue;
struct FSceneViewProjectionData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightPlayerHealthChanged, float, Current, float, Max);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightPlayerDamaged, float, AppliedDamage);

// 玩家角色：负责移动、鼠标准星瞄准和带瞄准偏移的俯视镜头。
UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightCharacter : public ACharacter, public ICandyFightCombatOwner
{
	GENERATED_BODY()

public:
	ACandyFightCharacter();
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* InstigatorController, AActor* DamageCauser) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Health", meta=(ClampMin="0.01"))
	float MaxHealth = 100.f;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="CandyFight|Health")
	float Health = 100.f;

	UPROPERTY(BlueprintAssignable, Category="CandyFight|Health")
	FCandyFightPlayerHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="CandyFight|Health")
	FCandyFightPlayerDamaged OnPlayerDamaged;

	/** Restore health, including from zero; returns the amount actually restored. */
	UFUNCTION(BlueprintCallable, Category="CandyFight|Health")
	float Heal(float Amount);

	virtual USceneComponent* GetCombatMount() const override;
	virtual bool GetCombatAim(FVector& Location, uint64& Frame) const override;
	virtual bool IsCombatShotAllowed() const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Interaction")
	TObjectPtr<UCandyFightSearchComponent> Search;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Escape")
	TObjectPtr<UCandyFightEscapeComponent> Escape;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Inventory")
	TObjectPtr<UCandyFightInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Combat")
	TObjectPtr<UCandyFightCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Camera")
	TObjectPtr<UCandyFightFireCameraComponent> FireCameraFeedback;

	UFUNCTION(Exec)
	void WeaponTestEquip(int32 WeaponNumber);

	/** 调试用元素切换：1=火 2=水 3=冰，其他值循环。 */
	UFUNCTION(Exec)
	void ElementTestSwitch(int32 ElementIndex);

	/** 测试用：直接放入一件物品，例如 InventoryTestAdd MintCandy。 */
	UFUNCTION(Exec)
	void InventoryTestAdd(const FString& ItemId);

	/** 测试用：在角色前方生成一件可拾取物品。 */
	UFUNCTION(Exec)
	void InventoryTestSpawn(const FString& ItemId);

	uint64 LastAimUpdateFrame = MAX_uint64;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Configuration")
	TObjectPtr<UCandyFightMovementConfig> MovementConfig; // 移动与碰撞尺寸配置。

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Configuration")
	TObjectPtr<UCandyFightCameraConfig> CameraConfig; // 镜头距离、角度与平滑配置。

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Input")
	TObjectPtr<UInputAction> MoveInputAction; // 二维移动输入：X 为左右，Y 为前后。

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Input")
	TObjectPtr<UInputMappingContext> MovementMappingContext; // WASD 和左摇杆的按键映射资产。

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Components")
	TObjectPtr<UStaticMeshComponent> CapsuleBody; // 可见胶囊的圆柱部分，不参与碰撞。

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Components")
	TObjectPtr<UStaticMeshComponent> CapsuleTop; // 可见胶囊的顶部球体。

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Components")
	TObjectPtr<UStaticMeshComponent> CapsuleBottom; // 可见胶囊的底部球体。

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Components")
	TObjectPtr<USceneComponent> WeaponHoldPoint; // 在角色蓝图视口中编辑持枪位置，不由配置或运行逻辑重置。

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="CandyFight|Configuration")
	int32 WeaponHoldPointVersion = 0; // 资产迁移标记。

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	FVector AimWorldLocation = FVector::ZeroVector; // 枪口高度（无枪时持枪点高度）平面上的准星世界位置。

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	FVector CombatAimWorldLocation = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	bool bAimAssistActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	TWeakObjectPtr<AActor> AimAssistTarget;

    void UpdateCombatAim(const FSceneViewProjectionData& Projection, uint64 Frame, float DeltaTime = 0.f);
	void RevalidateCombatAim();
	void ResetAimAssist();

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	bool bHasValidAim = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	FVector CurrentAimCameraOffset = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	TObjectPtr<UCandyFightCrosshairWidget> CrosshairWidget;

	/** Select a Designer-authored crosshair; unset uses the native fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Aiming")
	TSubclassOf<UCandyFightCrosshairWidget> CrosshairWidgetClass;

	/** Blueprint and console entry point for the currently displayed crosshair. */
	UFUNCTION(BlueprintCallable, Exec, Category="CandyFight|Aiming")
	void CrosshairSpread(float NewSpread);

	/** Multiplier for mouse-driven crosshair movement; can be changed during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CandyFight|Aiming",
		meta=(DisplayName="Crosshair Movement Sensitivity", ClampMin="0", UIMin="0", UIMax="30", Delta="0.1"))
	float CrosshairMouseSensitivity = 20.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	FVector2D AimScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Aiming")
	FVector2D RawAimScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Components")
	TObjectPtr<USpringArmComponent> CameraBoom; // 控制镜头距离、固定朝向和平滑跟随。

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Components")
	TObjectPtr<UCameraComponent> FollowCamera; // 实际输出玩家画面的透视摄像机。

	// 将两份 DataAsset 的参数应用到角色组件，也可从蓝图或编辑器调用。
	UFUNCTION(BlueprintCallable, CallInEditor, Category="CandyFight|Configuration")
	bool ApplyConfiguration();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    friend class FCandyFightAimAssistCombatTest;
    friend class FCandyFightAimAssistReleaseCameraTest;
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="CandyFight|Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> RuntimeMappingContext; // 当前玩家的临时映射副本，不保存到资产。

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem; // 用于移除已注册的映射。
	bool bConfigurationReady = false; // 配置有效时才允许处理移动输入。
	bool bCrosshairPositionInitialized = false;
	FCandyFightCrosshairSnapState CrosshairSnapState;
	FCandyFightAimFramingState AimFramingState;
	uint64 LastAimAssistFrame = MAX_uint64;
	TWeakObjectPtr<UCandyFightWeaponConfig> AimAssistConfig;
    FVector AimAssistWorldLocation = FVector::ZeroVector;
    TSharedPtr<FSceneViewProjectionData> AimAssistProjection;
	FCandyFightAimAssistDebug AimAssistDebug;

	bool ValidateConfiguration(FString& OutError) const;
	void UpdateGamepadDeadZone();
	void RemoveInputMapping();
	void CreateCrosshairWidget(APlayerController* PlayerController);
	void RemoveCrosshairWidget();
	void UpdateCrosshairScreenPosition(APlayerController* PlayerController, const FIntRect& ViewRect);
    void ReleaseAimAssist();
    void RefreshCombatAim();
    void SyncCombatAimRotation();
	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
};
