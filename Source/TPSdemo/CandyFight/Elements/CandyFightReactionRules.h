#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFightReactionRules.generated.h"

/**
 * 反应规则表的一行。等价于设计文档里的 DT_ReactionRules：
 * 只描述“哪两种元素相遇、触发什么反应、伤害乘区多少、双方各消耗多少 GU”，
 * 不含任何计时、状态或表现逻辑。新增反应（超载/感电）只需加一行。
 */
USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightReactionRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair")
    ECandyFightElement ElementA = ECandyFightElement::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair")
    ECandyFightElement ElementB = ECandyFightElement::Water;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair")
    ECandyFightReaction Reaction = ECandyFightReaction::Vaporize;

    /** A 作为触发元素、B 作为已有附着时的伤害倍率（火打水蒸发 1.5）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0"))
    float MultiplierATriggersB = 1.f;

    /** B 作为触发元素、A 作为已有附着时的伤害倍率（水打火蒸发 2.0）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0"))
    float MultiplierBTriggersA = 1.f;

    /** 每 1 GU 触发元素消耗的附着量。1.0 表示 1:1 对消。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumption", meta = (ClampMin = "0"))
    float AuraCostPerGauge = 1.f;

    /** 每 1 GU 被消耗的附着量额外吃掉多少触发元素。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumption", meta = (ClampMin = "0"))
    float TriggerCostPerAura = 1.f;

    /** 冻结类反应直接清空全部附着，不走按比例消耗。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumption")
    bool bConsumeAllAura = false;

    /** 冻结基础时长（秒）；0 表示该行不是冻结反应。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze", meta = (ClampMin = "0"))
    float FreezeSeconds = 0.f;

    /** 冻元素量 = 被消耗附着量 x 该系数（原神约 2 倍）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Freeze", meta = (ClampMin = "0"))
    float FreezeGaugeMultiplier = 2.f;

    /** 固定伤害修正，例如碎冰额外伤害；0 表示不附加。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float FlatDamageBonus = 0.f;

    static FCandyFightReactionRule MakeVaporize();
    static FCandyFightReactionRule MakeMelt();
    static FCandyFightReactionRule MakeFreeze();

    /** 元素对无序匹配。 */
    bool Matches(ECandyFightElement First, ECandyFightElement Second) const;
    /** 根据“已有附着 + 触发元素”的方向取倍率。 */
    float GetMultiplier(ECandyFightElement AuraElement, ECandyFightElement TriggerElement) const;
};

/**
 * 可编辑的反应规则集。组件未指定资产时使用类默认值（内置蒸发/融化/冻结三行），
 * 因此不创建任何资产也能直接跑起来；需要调参时新建一个 DataAsset 覆盖即可。
 */
UCLASS(BlueprintType)
class TPSDEMO_API UCandyFightReactionRules : public UDataAsset
{
    GENERATED_BODY()

public:
    UCandyFightReactionRules();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
    TArray<FCandyFightReactionRule> Rules;

    const FCandyFightReactionRule* FindRule(ECandyFightElement First, ECandyFightElement Second) const;
    const FCandyFightReactionRule* FindRule(ECandyFightReaction Reaction) const;
    static const UCandyFightReactionRules& GetDefaultRules();
};
