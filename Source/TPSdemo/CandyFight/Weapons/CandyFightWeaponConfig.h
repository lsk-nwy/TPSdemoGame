#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CandyFightWeaponConfig.generated.h"

class ACandyFightWeapon;
class USoundBase;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class ECandyFightFireMode : uint8 { SemiAutomatic, Automatic, Shotgun };

UENUM(BlueprintType)
enum class ECandyFightReloadPhase : uint8 { Ready, Magazine, Shell, Rack UMETA(Hidden), ShellStart };

/** Editable weapon definition. Runtime ammunition never lives in this asset. */
UCLASS(BlueprintType)
class TPSDEMO_API UCandyFightWeaponConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    FText DisplayName = FText::FromString(TEXT("Semi-auto"));
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    TSubclassOf<ACandyFightWeapon> WeaponClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    ECandyFightFireMode FireMode = ECandyFightFireMode::SemiAutomatic;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.01", ClampMax="60"))
    float ShotsPerSecond = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1"))
    int32 MagazineCapacity = 12;
    /** Full magazine reload duration; for shotguns, the duration of each shell insertion. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.01", Units="s"))
    float ReloadSeconds = 1.2f;
    /** Shotgun-only preparation before the first shell. Zero skips preparation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0", Units="s", EditCondition="FireMode == ECandyFightFireMode::Shotgun"))
    float ShellReloadStartDelay = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<USoundBase> FireSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<USoundBase> ReloadSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<USoundBase> ShellSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<USoundBase> DryFireSound;
    /** Legacy serialization only; racking is no longer used. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category="Legacy (Unused)", meta=(ToolTip="Deprecated compatibility field. Never played.")) TObjectPtr<USoundBase> RackSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0")) float FireVolume = .7f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0")) float ReloadVolume = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0")) float ShellVolume = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0")) float DryFireVolume = .4f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category="Legacy (Unused)", meta=(ToolTip="Deprecated compatibility field. Has no gameplay effect.")) float RackVolume = .5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category="Legacy (Unused)", meta=(ToolTip="Deprecated compatibility field. Has no gameplay effect.")) float RackSeconds = 1.352585f;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Audio") int32 AudioMigrationVersion = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0"))
    float Damage = 20.f;
    /** 每发命中写入目标的元素附着量（Gauge Units）；元素种类由当前枪械元素决定。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Element", meta=(ClampMin="0.1", ClampMax="4"))
    float ElementGauge = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1", Units="cm"))
    float Range = 5000.f;
    /** Player crosshair attraction; AI continues to use its own combat aim. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim Assist")
    bool bEnableAimAssist = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim Assist", meta=(ClampMin="0", ToolTip="Extra tolerance in viewport pixels at 1080p; scales with view height.", EditCondition="bEnableAimAssist"))
    float AimAssistPaddingPixels = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim Assist", meta=(ClampMin="0", ToolTip="Screen-distance advantage needed to switch equal-priority targets, in 1080p pixels.", EditCondition="bEnableAimAssist"))
    float AimAssistSwitchBiasPixels = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim Assist", meta=(ClampMin="0", Units="s", ToolTip="Time to remove 95 percent of crosshair error; zero snaps instantly.", EditCondition="bEnableAimAssist"))
    float AimAssistSnapSeconds = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aim Assist", meta=(ClampMin="0", ToolTip="Extra 1080p pixels beyond acquisition padding to retain the current target.", EditCondition="bEnableAimAssist"))
    float AimAssistReleasePaddingPixels = 24.f;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Aim Assist")
    int32 AimAssistMigrationVersion = 0;
    /** 子弹水平飞行速度，5000 cm/s = 50 米/秒。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.01", Units="cm/s"))
    float ProjectileSpeed = 5000.f;
    /** 曳光最大长度；0 关闭显示，不影响命中。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0", Units="cm"))
    float TracerLength = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.01"))
    float TracerThickness = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile")
    FLinearColor TracerColor = FLinearColor(1.f, .85f, .25f);
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Projectile")
    int32 ProjectileMigrationVersion = 0;
    /** 抵达阻挡物时播放；清空可关闭，不影响伤害。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact VFX")
    TObjectPtr<UNiagaraSystem> ImpactEffect;
    /** 命中粒子的统一缩放；发射时保存快照。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact VFX", meta=(ClampMin="0.001"))
    float ImpactEffectScale = 1.f;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Impact VFX")
    int32 ImpactVFXMigrationVersion = 0;
    /** 只改变最终画面，不影响准星、逻辑瞄准和弹道。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Shake")
    bool bEnableFireCameraShake = true;
    /** 后坐位移每轴上限；X 为相机右向、Y 为上向，两分量同时应用，单位 cm。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Shake", meta=(ClampMin="0", Units="cm"))
    FVector2D FireShakeMaxOffset = FVector2D(4., 4.);
    /** 以射击反方向的画面平面投影为中心，随机偏转的最大角度；0 表示严格反向。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Shake", meta=(ClampMin="0", ClampMax="45", Units="deg"))
    float FireShakeDirectionJitterDegrees = 15.f;
    /** 绕视线方向的最大随机歪斜角度，不改变俯仰和水平朝向。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Shake", meta=(ClampMin="0", Units="deg"))
    float FireShakeMaxRollDegrees = .3f;
    /** 每发刷新后平滑归零的游戏时间；新的射击替换旧值，不累加。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Shake", meta=(ClampMin="0.001", Units="s"))
    float FireShakeReturnSeconds = .25f;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Camera Shake")
    int32 FireCameraMigrationVersion = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1", ClampMax="64"))
    int32 PelletCount = 1;
    /** Fraction of the whole horizontal fan assigned to the central sector. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(UIMin="0.01", UIMax="0.99", EditCondition="FireMode == ECandyFightFireMode::Shotgun"))
    float ShotgunCenterWidthRatio = .5f;
    /** Pellets in each side; the remaining pellets are sampled in the center. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(ClampMin="0", EditCondition="FireMode == ECandyFightFireMode::Shotgun"))
    int32 ShotgunSidePelletCount = 2;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Spread") int32 ShotgunPatternMigrationVersion = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(ClampMin="0", ClampMax="45", Units="deg"))
    float BaseSpread = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(ClampMin="0", ClampMax="45", Units="deg"))
    float MaxSpread = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(ClampMin="0", Units="deg"))
    float SpreadPerShot = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(ClampMin="0", Units="s"))
    float RecoveryDelay = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spread", meta=(ClampMin="0", Units="deg/s"))
    float RecoveryPerSecond = 5.f;

    bool ValidateConfiguration(FString& Error) const;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Per-definition state, retained when the console equips another definition. */
USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightWeaponState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Ammo = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Spread = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReloading = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) ECandyFightReloadPhase ReloadPhase = ECandyFightReloadPhase::Ready;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNeedsRack = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDryFiring = false;
    int32 PhaseSerial = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShotsFired = 0;
    double NextShotTime = 0.;
    double LastShotTime = -100000.;
    double LastUpdateTime = 0.;
    double ReloadStartTime = 0.;
    double ReloadEndTime = 0.;

    void Initialize(const UCandyFightWeaponConfig& Config, double Now);
    bool Advance(const UCandyFightWeaponConfig& Config, double Now);
    bool CanFire(const UCandyFightWeaponConfig& Config, double Now) const;
    void CommitShot(const UCandyFightWeaponConfig& Config, double Now, bool bTriggerHeld = false);
    bool BeginReload(const UCandyFightWeaponConfig& Config, double Now);
    void CancelReload();
    void ResumeEquipped(const UCandyFightWeaponConfig& Config, double Now);
    void ReleaseTrigger(const UCandyFightWeaponConfig& Config, double Now);
private:
    void SetPhase(ECandyFightReloadPhase Phase, double Now, double Duration);
};

/** Game-time audio scheduling, independent of hardware and audio-finished callbacks. */
struct TPSDEMO_API FCandyFightAudioSchedule
{
    int32 ActionSerial = INDEX_NONE;
    double NextDryTime = 0.;
    bool bWasDry = false;
    void Reset() { *this = FCandyFightAudioSchedule(); }
    bool ConsumeAction(const FCandyFightWeaponState& State, double Now, double Duration, double& Offset);
    bool ConsumeDry(const FCandyFightWeaponState& State, double Now, double Duration);
};

/** A semi-auto click during cooldown is discarded rather than queued. */
struct TPSDEMO_API FCandyFightFireIntent
{
    bool bHeld = false;
    bool bPendingPress = false;
    void Press(bool bCanFire) { if (!bHeld) { bHeld = true; bPendingPress = bCanFire; } }
    void Release() { bHeld = false; } // A down/up in one frame is still a click.
    void Reset() { bHeld = bPendingPress = false; }
    bool Consume(ECandyFightFireMode Mode)
    {
        const bool Result = bPendingPress || (Mode == ECandyFightFireMode::Automatic && bHeld);
        bPendingPress = false;
        return Result;
    }
};

namespace CandyFightWeapons
{
    constexpr ECollisionChannel TraceChannel = ECC_GameTraceChannel1;
    TPSDEMO_API void SampleAngles(const UCandyFightWeaponConfig& Config, float Spread, FRandomStream& Random, TArray<float>& OutAngles);
}
