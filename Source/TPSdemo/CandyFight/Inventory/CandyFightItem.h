#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CandyFightItem.generated.h"

/** 物品使用效果。速度效果使用 EffectAmount 作为倍率增量（0.4 表示 +40%）。 */
UENUM(BlueprintType)
enum class ECandyFightItemEffect : uint8
{
    None,
    Heal,
    Speed,
    Gold
};

/** 一件物品的静态定义。运行时物品只保存 Id，不复制资源。 */
USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Item")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category="Item")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Item")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category="Item")
    ECandyFightItemEffect Effect = ECandyFightItemEffect::None;

    /** Heal/Gold 为数值；Speed 为倍率增量。 */
    UPROPERTY(BlueprintReadOnly, Category="Item")
    float EffectAmount = 0.f;

    /** Speed 的持续时间（秒）；其他效果不使用。 */
    UPROPERTY(BlueprintReadOnly, Category="Item")
    float EffectDuration = 0.f;

    /** 场景中掉落物的颜色。 */
    UPROPERTY(BlueprintReadOnly, Category="Item")
    FLinearColor Color = FLinearColor::White;

    /** 世界模型缩放倍率。 */
    UPROPERTY(BlueprintReadOnly, Category="Item")
    float WorldScale = 1.f;

    bool IsValid() const { return !Id.IsNone(); }
};

/**
 * 演示用物品目录。使用原生 C++ 定义，避免为了测试物品额外制作 DataAsset；
 * 以后替换成数据表时，只需要保留 FindItem / GetCatalog 的接口。
 */
UCLASS()
class TPSDEMO_API UCandyFightItemLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="CandyFight|Items")
    static TArray<FCandyFightItemDefinition> GetCatalog();

    UFUNCTION(BlueprintPure, Category="CandyFight|Items")
    static bool FindItem(FName ItemId, FCandyFightItemDefinition& OutItem);

    UFUNCTION(BlueprintPure, Category="CandyFight|Items")
    static TArray<FName> GetTestItemIds();

    UFUNCTION(BlueprintPure, Category="CandyFight|Items")
    static bool IsUsable(const FCandyFightItemDefinition& Item);

    UFUNCTION(BlueprintPure, Category="CandyFight|Items")
    static FText DescribeEffect(const FCandyFightItemDefinition& Item);
};
