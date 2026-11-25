#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFightElementAuraComponent.generated.h"

class UTextRenderComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCandyFightAuraChanged, ECandyFightElement, Element, float, Gauge, float, MaxGauge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightAuraExpired, ECandyFightElement, Element);

/**
 * 元素附着层：只维护“身上有什么元素、还剩多少 GU、多久自然消失”。
 * 不查反应表、不算伤害、不碰冻结状态——这些都由反应结算层负责。
 * 挂到任意可受击 Actor（敌人、靶子）即可工作。
 */
UCLASS(ClassGroup = (CandyFight), meta = (BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightElementAuraComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCandyFightElementAuraComponent();

    /** 1 GU 的自然衰减时长；默认 9.5 秒。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura", meta = (ClampMin = "0.1", Units = "s"))
    float SecondsPerGauge = 9.5f;

    /** 超过 1 GU 时每多 1 GU 增加的衰减时长；1U→9.5s、2U→12s。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura", meta = (ClampMin = "0", Units = "s"))
    float AdditionalSecondsPerGauge = 2.5f;

    /** 单元素附着上限（GU）。重复施加同一元素时叠加并刷新衰减。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura", meta = (ClampMin = "0.1"))
    float MaxAuraGauge = 2.f;

    /** 元素 -> 附着记录。只读给外部查询，写入必须走 ApplyAura/ConsumeAura。 */
    UPROPERTY(VisibleAnywhere, Category = "Aura")
    TMap<ECandyFightElement, FCandyFightAuraEntry> Auras;

    /** 当前附着对应的 GameplayTag（Element.Fire / Element.Water / Element.Ice）。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aura")
    FGameplayTagContainer ActiveTags;

    UPROPERTY(BlueprintAssignable, Category = "Aura")
    FCandyFightAuraChanged OnAuraChanged;

    UPROPERTY(BlueprintAssignable, Category = "Aura")
    FCandyFightAuraExpired OnAuraExpired;

    /** 写入附着：叠加、夹到上限、按新的总 GU 重算衰减速率。 */
    UFUNCTION(BlueprintCallable, Category = "Aura")
    void ApplyAura(ECandyFightElement Element, float Gauge);

    /** 按量消耗附着，返回实际消耗量；归零时移除并广播。 */
    UFUNCTION(BlueprintCallable, Category = "Aura")
    float ConsumeAura(ECandyFightElement Element, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Aura")
    void ClearAura(ECandyFightElement Element);

    UFUNCTION(BlueprintCallable, Category = "Aura")
    void ClearAllAuras();

    UFUNCTION(BlueprintPure, Category = "Aura")
    float GetAuraGauge(ECandyFightElement Element) const;

    UFUNCTION(BlueprintPure, Category = "Aura")
    bool HasAura(ECandyFightElement Element) const;

    /** 当前附着量最高的元素；没有附着时返回 None。 */
    UFUNCTION(BlueprintPure, Category = "Aura")
    ECandyFightElement GetStrongestAura() const;

    UFUNCTION(BlueprintPure, Category = "Aura")
    FCandyFightAuraEntry GetAura(ECandyFightElement Element) const;

    /** 自然衰减到指定的游戏时间；由 Tick 每帧调用，也供无 PIE 的确定性测试驱动。 */
    void AdvanceTo(double Now);

    /** 纯函数：初始 GU -> 自然消失所需秒数（1U=9.5s，2U=12s）。 */
    static float GetDecaySeconds(float InitialGauge, float SecondsPerGauge, float AdditionalSecondsPerGauge, float MaxAuraGauge);

    /** 世界空间状态文字：FIRE 1.0U；用于无美术资源时也能看到附着。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display")
    bool bShowAuraLabel = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display", meta = (ClampMin = "1", Units = "cm"))
    float AuraLabelHeight = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display", meta = (ClampMin = "1"))
    float AuraLabelSize = 30.f;

    /** 头顶元素光球：当前带什么元素就亮什么颜色，远距离也能一眼看到。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display")
    bool bShowAuraOrbs = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display", meta = (ClampMin = "4", Units = "cm"))
    float AuraOrbSize = 26.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display", meta = (ClampMin = "0", Units = "cm"))
    float AuraOrbHeight = 190.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aura|Display", meta = (ClampMin = "1", Units = "cm"))
    float AuraOrbSpacing = 34.f;

    /** 供测试/调试读取的当前标签文本。 */
    const FString& GetAuraLabelText() const { return AuraLabelText; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextRenderComponent> AuraLabel;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> AuraOrbMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> AuraOrbMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> AuraOrbs;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> AuraOrbColors;

    double LastAdvanceTime = 0.;
    FString AuraLabelText;

    void AdvanceToNow();
    void RefreshTags();
    void EnsureLabel();
    void UpdateLabel();
    void EnsureOrbs();
    void UpdateOrbs();
    void FaceCamera();
    void RemoveAuraEntry(ECandyFightElement Element, bool bExpired);
};
