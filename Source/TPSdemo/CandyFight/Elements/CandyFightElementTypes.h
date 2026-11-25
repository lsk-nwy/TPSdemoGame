#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "CandyFightElementTypes.generated.h"

/** 玩家枪械可切换的三种元素；None 表示物理（无附着）。 */
UENUM(BlueprintType)
enum class ECandyFightElement : uint8
{
    None UMETA(DisplayName = "None"),
    Fire UMETA(DisplayName = "Fire"),
    Water UMETA(DisplayName = "Water"),
    Ice UMETA(DisplayName = "Ice")
};

/** 反应结算层输出的反应类型。Freeze 走状态路径，其余走伤害乘区路径。 */
UENUM(BlueprintType)
enum class ECandyFightReaction : uint8
{
    None UMETA(DisplayName = "None"),
    Vaporize UMETA(DisplayName = "Vaporize"),
    Melt UMETA(DisplayName = "Melt"),
    Freeze UMETA(DisplayName = "Freeze"),
    Shatter UMETA(DisplayName = "Shatter")
};

/**
 * 附着层的一条记录：只描述“有什么元素、还剩多少 GU、每秒衰减多少”，
 * 不包含任何反应或伤害逻辑。
 */
USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightAuraEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aura")
    ECandyFightElement Element = ECandyFightElement::None;

    /** 当前剩余附着量（Gauge Units）。1U 约 9.5 秒自然消失，2U 约 12 秒。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aura")
    float Gauge = 0.f;

    /** 每秒衰减量，在施加/叠加时按当时的 GU 一次性算好。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aura")
    float DecayPerSecond = 0.f;

    bool IsActive() const { return Element != ECandyFightElement::None && Gauge > KINDA_SMALL_NUMBER; }
};

/**
 * 反应结算层的返回值。它由附着层+规则表算出，供伤害/状态两条路径消费。
 * 相当于 GAS 里 ExecCalc 的输出，但不依赖 GAS。
 */
USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightReactionHit
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    ECandyFightElement Element = ECandyFightElement::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    ECandyFightReaction Reaction = ECandyFightReaction::None;

    /** 伤害乘区；无反应时为 1。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    float Multiplier = 1.f;

    /** 固定伤害修正（碎冰等）。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    float BonusDamage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    float BaseDamage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    float FinalDamage = 0.f;

    /** 本次被反应消耗掉的旧附着量。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    float AuraConsumed = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reaction")
    bool bFreezeApplied = false;

    bool HasReaction() const { return Reaction != ECandyFightReaction::None; }
};

UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Element_Fire);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Element_Water);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Element_Ice);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Reaction_Vaporize);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Reaction_Melt);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Reaction_Freeze);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_Reaction_Shatter);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(CandyFightTags_State_Frozen);

/** 元素/反应的类型辅助函数；不包含任何状态，便于测试与蓝图共用。 */
namespace CandyFightElements
{
    TPSDEMO_API bool IsValidElement(ECandyFightElement Element);
    TPSDEMO_API FGameplayTag GetElementTag(ECandyFightElement Element);
    TPSDEMO_API FGameplayTag GetReactionTag(ECandyFightReaction Reaction);
    /** 中文显示名，用于 HUD。 */
    TPSDEMO_API FText GetElementName(ECandyFightElement Element);
    TPSDEMO_API FText GetReactionName(ECandyFightReaction Reaction);
    /** ASCII 短名，用于世界空间 TextRender / 日志。 */
    TPSDEMO_API const TCHAR* GetElementId(ECandyFightElement Element);
    TPSDEMO_API const TCHAR* GetReactionId(ECandyFightReaction Reaction);
    TPSDEMO_API FLinearColor GetElementColor(ECandyFightElement Element);
    TPSDEMO_API FLinearColor GetReactionColor(ECandyFightReaction Reaction);
    /** 切换顺序：火 -> 水 -> 冰 -> 火。 */
    TPSDEMO_API ECandyFightElement GetNextElement(ECandyFightElement Element);
    TPSDEMO_API FString FormatElementShortName(ECandyFightElement Element);
}
