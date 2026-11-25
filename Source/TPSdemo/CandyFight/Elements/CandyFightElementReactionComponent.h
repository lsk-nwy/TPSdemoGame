#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFight/Elements/CandyFightReactionRules.h"
#include "CandyFightElementReactionComponent.generated.h"

class UCandyFightElementAuraComponent;
class UCandyFightFrozenEffectComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightReactionResolved, FCandyFightReactionHit, Hit);

/**
 * 反应结算层：读取“旧附着 + 新元素”，查规则表决定反应、倍率与 GU 消耗，
 * 并把结果分成两条路径——蒸发/融化走伤害乘区，冻结/碎冰走状态层。
 * 不负责计时衰减（附着层负责），也不负责冻结表现（状态层负责）。
 */
UCLASS(ClassGroup = (CandyFight), meta = (BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightElementReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCandyFightElementReactionComponent();

    /** 规则表；为空时使用类默认值（内置蒸发/融化/冻结）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
    TObjectPtr<UCandyFightReactionRules> Rules;

    /** 反应标签的持续时间，便于特效/UI 用 GameplayTag 查询“刚刚发生了反应”。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", Units = "s"))
    float ReactionTagSeconds = .75f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    FGameplayTagContainer ActiveReactionTags;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    ECandyFightReaction LastReaction = ECandyFightReaction::None;

    UPROPERTY(BlueprintAssignable, Category = "Reaction")
    FCandyFightReactionResolved OnReaction;

    /**
     * 结算一次命中：写入新元素、触发反应、消耗 GU、应用冻结，并返回最终伤害。
     * 这是整个系统唯一的“组合匹配”入口，调用方（TakeDamage）不需要知道任何反应细节。
     */
    UFUNCTION(BlueprintCallable, Category = "Reaction")
    FCandyFightReactionHit SettleHit(ECandyFightElement IncomingElement, float IncomingGauge, float BaseDamage);

    /** 推进反应标签/飘字计时；Tick 与确定性测试共用。 */
    void AdvanceTo(double Now);

    UFUNCTION(BlueprintPure, Category = "Reaction")
    UCandyFightElementAuraComponent* GetAuraComponent() const;

    UFUNCTION(BlueprintPure, Category = "Reaction")
    UCandyFightFrozenEffectComponent* GetFrozenComponent() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Display")
    bool bShowReactionLabel = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Display", meta = (ClampMin = "1", Units = "cm"))
    float ReactionLabelHeight = 238.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Display", meta = (ClampMin = "1"))
    float ReactionLabelSize = 28.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Display", meta = (ClampMin = "0.01", Units = "s"))
    float ReactionLabelSeconds = 1.1f;

    const FString& GetReactionLabelText() const { return ReactionLabelText; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextRenderComponent> ReactionLabel;

    mutable TWeakObjectPtr<UCandyFightElementAuraComponent> Aura;
    mutable TWeakObjectPtr<UCandyFightFrozenEffectComponent> Frozen;

    double ReactionTagUntil = 0.;
    double ReactionLabelUntil = 0.;
    FString ReactionLabelText;

    void ResolveRefs() const;
    void FinalizeHit(FCandyFightReactionHit& Hit, double Now);
    void ShowReactionLabel(const FCandyFightReactionHit& Hit, double Now);
    void EnsureLabel();
    void UpdateLabelVisibility(double Now);
    void FaceCamera();
};
