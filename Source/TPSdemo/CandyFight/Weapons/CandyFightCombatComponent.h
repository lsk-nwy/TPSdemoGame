#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFightCombatComponent.generated.h"

class ACandyFightWeapon;
class ACandyFightProjectileShot;
class ACandyFightCharacter;
class APawn;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UCandyFightWeaponHUD;
class UMaterialInstanceDynamic;
class UAudioComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCandyFightCombatChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightShotEvent, int32, WeaponNumber, int32, Pellets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightHitEvent, AActor*, Target, float, Damage);

UCLASS(ClassGroup=(CandyFight), meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCandyFightCombatComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons") TArray<TObjectPtr<UCandyFightWeaponConfig>> WeaponConfigs;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> FireAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> ReloadAction;
    /** 可选：不指定时运行时创建 Q 键动作并注入映射上下文。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> ElementSwitchAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> CombatMappingContext;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI") TSubclassOf<UCandyFightWeaponHUD> HUDClass;
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") TObjectPtr<ACandyFightWeapon> EquippedWeapon;
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") int32 EquippedIndex = INDEX_NONE;
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") TArray<FCandyFightWeaponState> WeaponStates;
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") int32 TotalShotsFired = 0;
    /** Intended range endpoints at launch, not confirmed hit locations. */
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") TArray<FVector> LastShotEnds;
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") FVector LastShotStart = FVector::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly, Category="State") TObjectPtr<UCandyFightWeaponHUD> WeaponHUD;
    /** 当前枪械元素：Q 在火/水/冰之间循环；子弹在开火那一刻快照。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category="Element") ECandyFightElement CurrentElement = ECandyFightElement::Fire;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnElementChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnWeaponChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnAmmoChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnReloadChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightShotEvent OnShot;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightHitEvent OnHit;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnReloadPhaseChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnShellLoaded;
    UPROPERTY(BlueprintAssignable, Category="Events") FCandyFightCombatChanged OnDryFireChanged;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Audio Diagnostics") int32 FireSoundRequests = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Audio Diagnostics") int32 ActionSoundRequests = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Audio Diagnostics") int32 DrySoundRequests = 0;

    UFUNCTION(BlueprintCallable, Category="Combat") void StartFire();
    UFUNCTION(BlueprintCallable, Category="Combat") void StopFire();
    UFUNCTION(BlueprintCallable, Category="Combat") void Reload();
    UFUNCTION(BlueprintPure, Category="Combat") UCandyFightWeaponConfig* GetCurrentConfig() const;
    UFUNCTION(BlueprintPure, Category="Combat") int32 GetAmmo() const;
    UFUNCTION(BlueprintPure, Category="Combat") bool IsReloading() const;
    UFUNCTION(BlueprintPure, Category="Combat") ECandyFightReloadPhase GetReloadPhase() const;
    UFUNCTION(BlueprintPure, Category="Combat", meta=(DeprecatedFunction, DeprecationMessage="Racking was removed; this always returns false.")) bool IsRacking() const;
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDryFiring() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetReloadProgress() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetSpread() const;
    UFUNCTION(BlueprintPure, Category="Combat") bool IsCombatInputAllowed() const;
    UFUNCTION(BlueprintPure, Category="Element") ECandyFightElement GetElement() const { return CurrentElement; }
    /** 直接指定元素；返回 false 表示组件已关闭或元素非法。 */
    UFUNCTION(BlueprintCallable, Category="Element") bool SetElement(ECandyFightElement NewElement);
    /** 火 -> 水 -> 冰 -> 火，同时刷新枪身颜色与 HUD。 */
    UFUNCTION(BlueprintCallable, Category="Element") void CycleElement();

    // 外部驱动仍经由 StartFire/StopFire，不绕过射速、装填和本帧瞄准检查。
    bool IsExternallyDriven() const;
    void ShutdownCombat();
    // Reversible interruption; health/search never uses permanent shutdown.
    void SuspendForSearch();
    void ClearExternalFireRequest();
    bool EquipTestWeapon(int32 OneBasedIndex);
    void InitializeLocalPlayer();
    void BindInput(UEnhancedInputComponent* Input);
    double GetAimPlaneZ(double Fallback) const;
    static void TracePellet(UWorld* World, const FVector& Start, const FVector& Direction, float Range,
        const FCollisionQueryParams& Params, FHitResult& Hit, FVector& End);

protected:
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    friend class FCandyFightAimAssistCombatTest;
    friend class FCandyFightAimAssistReleaseCameraTest;
    friend class FCandyFightPlayerHealthProjectileTest;
    friend class FCandyFightCombatShutdownTest;
    friend class FCandyFightAutonomyTest;
    friend class FCandyFightEnemyWeaponIntegrationTest;
    friend class FCandyFightAudioIntegrationTest;
    friend class FCandyFightProjectileCombatTest;
    friend class FCandyFightFireCameraIntegrationTest;
    TArray<TWeakObjectPtr<ACandyFightProjectileShot>> ActiveShots;
    void ClearProjectiles();
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RegisteredMapping;
    UPROPERTY(Transient) TObjectPtr<UInputAction> RuntimeElementAction;
    FCandyFightFireIntent FireIntent;
    FRandomStream Random;
    uint64 LastFireFrame = MAX_uint64;
    bool bReportedInvalidConfig = false;
    bool bFireNeedsRelease = false;
    // EndPlay、失去控制和 GC 均可能清理同一组件；关闭后不再执行游戏状态转换。
    bool bCombatShutdown = false;
    FDelegateHandle ActivationHandle;
    FCandyFightAudioSchedule AudioSchedule;
    TWeakObjectPtr<UAudioComponent> ActionAudio;
    TWeakObjectPtr<UAudioComponent> DryAudio;
    TArray<TWeakObjectPtr<UAudioComponent>> ShotAudio;
    TArray<TWeakObjectPtr<UAudioComponent>> FadingActionAudio;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ElementMaterialInstances;
    int32 ObservedPhaseSerial = INDEX_NONE;
    bool bObservedDry = false;
    void SyncStateEvents();
    void ApplyElementTint();
    void HandleElementSwitch();
    UInputAction* GetElementSwitchAction();
    void ReleaseFireIntent();
    void UpdateWeaponAudio(double Now);
    void PlayFireAudio();
    UAudioComponent* CreateWeaponAudio(USoundBase* Sound, float Volume, float Offset = 0.f, bool bFollowOwner = false);
    void StopWeaponAudio();
    void CancelActionAudio();
    static void StopAudio(TWeakObjectPtr<UAudioComponent>& Component);
    void OnApplicationActivationChanged(bool bActive);
    void HandlePress(const FInputActionValue& Value);
    void HandleRelease(const FInputActionValue& Value);
    void HandleReload(const FInputActionValue& Value);
    void FireShot(APawn& Character, double Now);
    void RemoveLocalPlayer();
};
