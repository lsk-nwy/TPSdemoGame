#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFightFrozenEffectComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UTextRenderComponent;

USTRUCT()
struct TPSDEMO_API FCandyFightFrozenMaterialBackup
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<UMeshComponent> Mesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> Materials;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightFrozenChanged, bool, bFrozen, float, RemainingSeconds);

/**
 * 冻结状态层：独立处理“不能移动/不能开枪、变蓝、寒气、受击提前解冻”，
 * 不参与伤害乘区，也不知道元素反应表。
 */
UCLASS(ClassGroup = (CandyFight), meta = (BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightFrozenEffectComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCandyFightFrozenEffectComponent();

    /** 冻结期间累计受到多少非火伤害后提前解冻；<=0 表示只能等自然解除。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen", meta = (ClampMin = "0"))
    float BreakDamageThreshold = 30.f;

    /** 冻结期间持续寒气；为空则不生成。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen")
    TObjectPtr<UNiagaraSystem> FreezeEffect;

    /** 解冻/碎冰时的一次性特效；为空则不生成。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen")
    TObjectPtr<UNiagaraSystem> ShatterEffect;

    /** 冰霜覆盖材质；为空时改为给原有材质做动态实例染色。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen")
    TObjectPtr<UMaterialInterface> FrostMaterial;

    /** 是否给 Owner 网格挂动态材质实例并染色。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen")
    bool bTintOwnerMaterials = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen")
    FName TintParameterName = TEXT("Color");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen")
    FLinearColor TintColor = FLinearColor(.55f, .9f, 1.f, 1.f);

    /** 世界空间状态文字，没有美术资源也能看见冻结中。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen|Display")
    bool bShowStateLabel = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen|Display", meta = (ClampMin = "1", Units = "cm"))
    float StateLabelHeight = 202.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frozen|Display", meta = (ClampMin = "1"))
    float StateLabelSize = 22.f;

    UPROPERTY(BlueprintReadOnly, Category = "Frozen")
    bool bFrozen = false;

    /** 冻元素量（GU），约为被消耗附着量的两倍。 */
    UPROPERTY(BlueprintReadOnly, Category = "Frozen")
    float FrozenGauge = 0.f;

    /** 冻结中累计的破冰伤害。 */
    UPROPERTY(BlueprintReadOnly, Category = "Frozen")
    float BreakDamageAccumulated = 0.f;

    /** State.Frozen；供其他系统/蓝图按 GameplayTag 查询。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Frozen")
    FGameplayTagContainer StateTags;

    UPROPERTY(BlueprintAssignable, Category = "Frozen")
    FCandyFightFrozenChanged OnFrozenChanged;

    /** 进入/刷新冻结。返回 false 表示参数非法或没有世界。 */
    UFUNCTION(BlueprintCallable, Category = "Frozen")
    bool Freeze(float Seconds, float Gauge);

    /** 解除冻结；bShatter 只影响一次性表现。 */
    UFUNCTION(BlueprintCallable, Category = "Frozen")
    void Unfreeze(bool bShatter);

    /** 冻结中受到伤害时调用；返回 true 表示本次伤害触发了提前解冻。 */
    bool NotifyDamage(float Damage);

    UFUNCTION(BlueprintPure, Category = "Frozen")
    bool IsFrozen() const { return bFrozen; }

    UFUNCTION(BlueprintPure, Category = "Frozen")
    float GetRemainingSeconds() const;

    float GetRemainingSecondsAt(double Now) const;

    /** 到期判定；Tick 每帧调用，也供无 PIE 的确定性测试驱动。 */
    void AdvanceTo(double Now);

    const FString& GetStateLabelText() const { return StateLabelText; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextRenderComponent> StateLabel;

    UPROPERTY(Transient)
    TArray<FCandyFightFrozenMaterialBackup> MaterialBackups;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> FrostMIDs;

    TWeakObjectPtr<UNiagaraComponent> ActiveFreezeEffect;
    TWeakObjectPtr<UNiagaraComponent> ActiveShatterEffect;
    double FrozenUntil = 0.;
    uint8 SavedMovementMode = 0;
    bool bHasSavedMovementMode = false;
    FString StateLabelText;

    void ApplyState();
    void RestoreState();
    void RemoveStateTag();
    void TintMaterials();
    void RestoreMaterials();
    void EnsureLabel();
    void UpdateLabel();
    void FaceCamera();
};
