#include "CandyFight/Inventory/CandyFightItem.h"

namespace
{
    FCandyFightItemDefinition MakeItem(
        FName Id,
        const FText& DisplayName,
        const FText& Description,
        ECandyFightItemEffect Effect,
        float Amount,
        float Duration,
        const FLinearColor& Color,
        float WorldScale = 1.f)
    {
        FCandyFightItemDefinition Item;
        Item.Id = Id;
        Item.DisplayName = DisplayName;
        Item.Description = Description;
        Item.Effect = Effect;
        Item.EffectAmount = Amount;
        Item.EffectDuration = Duration;
        Item.Color = Color;
        Item.WorldScale = WorldScale;
        return Item;
    }

    const TArray<FCandyFightItemDefinition>& GetCatalogInternal()
    {
        static const TArray<FCandyFightItemDefinition> Catalog = []
        {
            TArray<FCandyFightItemDefinition> Items;
            Items.Add(MakeItem(TEXT("MintCandy"),
                NSLOCTEXT("CandyFightItems", "MintCandyName", "薄荷糖"),
                NSLOCTEXT("CandyFightItems", "MintCandyDescription", "清凉的糖块，恢复 25 点生命。"),
                ECandyFightItemEffect::Heal, 25.f, 0.f, FLinearColor(.35f, 1.f, .62f), 1.f));
            Items.Add(MakeItem(TEXT("EnergyDrink"),
                NSLOCTEXT("CandyFightItems", "EnergyDrinkName", "能量饮料"),
                NSLOCTEXT("CandyFightItems", "EnergyDrinkDescription", "糖分冲击：8 秒内移动速度提高 40%。"),
                ECandyFightItemEffect::Speed, .4f, 8.f, FLinearColor(.25f, .78f, 1.f), 1.1f));
            Items.Add(MakeItem(TEXT("GoldTicket"),
                NSLOCTEXT("CandyFightItems", "GoldTicketName", "金券"),
                NSLOCTEXT("CandyFightItems", "GoldTicketDescription", "可兑换的糖纸金券，使用后获得 30 金币。"),
                ECandyFightItemEffect::Gold, 30.f, 0.f, FLinearColor(1.f, .77f, .18f), .9f));
            Items.Add(MakeItem(TEXT("EmergencyKit"),
                NSLOCTEXT("CandyFightItems", "EmergencyKitName", "急救糖盒"),
                NSLOCTEXT("CandyFightItems", "EmergencyKitDescription", "大份急救糖，恢复 60 点生命。"),
                ECandyFightItemEffect::Heal, 60.f, 0.f, FLinearColor(1.f, .22f, .3f), 1.15f));
            Items.Add(MakeItem(TEXT("CaramelChunk"),
                NSLOCTEXT("CandyFightItems", "CaramelChunkName", "焦糖块"),
                NSLOCTEXT("CandyFightItems", "CaramelChunkDescription", "任务材料。无法使用，可以用来测试丢弃与重新拾取。"),
                ECandyFightItemEffect::None, 0.f, 0.f, FLinearColor(.85f, .48f, .18f), 1.f));
            return Items;
        }();
        return Catalog;
    }
}

TArray<FCandyFightItemDefinition> UCandyFightItemLibrary::GetCatalog()
{
    return GetCatalogInternal();
}

bool UCandyFightItemLibrary::FindItem(FName ItemId, FCandyFightItemDefinition& OutItem)
{
    if (ItemId.IsNone()) return false;
    for (const FCandyFightItemDefinition& Item : GetCatalogInternal())
    {
        if (Item.Id == ItemId)
        {
            OutItem = Item;
            return true;
        }
    }
    return false;
}

TArray<FName> UCandyFightItemLibrary::GetTestItemIds()
{
    TArray<FName> Ids;
    for (const FCandyFightItemDefinition& Item : GetCatalogInternal()) Ids.Add(Item.Id);
    return Ids;
}

bool UCandyFightItemLibrary::IsUsable(const FCandyFightItemDefinition& Item)
{
    return Item.Effect != ECandyFightItemEffect::None;
}

FText UCandyFightItemLibrary::DescribeEffect(const FCandyFightItemDefinition& Item)
{
    FFormatNamedArguments Args;
    switch (Item.Effect)
    {
    case ECandyFightItemEffect::Heal:
        Args.Add(TEXT("Amount"), FMath::RoundToInt(Item.EffectAmount));
        return FText::Format(NSLOCTEXT("CandyFightItems", "HealEffect", "恢复 {Amount} 点生命"), Args);
    case ECandyFightItemEffect::Speed:
        Args.Add(TEXT("Seconds"), FMath::RoundToInt(Item.EffectDuration));
        Args.Add(TEXT("Percent"), FMath::RoundToInt(Item.EffectAmount * 100.f));
        return FText::Format(NSLOCTEXT("CandyFightItems", "SpeedEffect", "{Seconds} 秒内移动速度 +{Percent}%"), Args);
    case ECandyFightItemEffect::Gold:
        Args.Add(TEXT("Amount"), FMath::RoundToInt(Item.EffectAmount));
        return FText::Format(NSLOCTEXT("CandyFightItems", "GoldEffect", "获得 {Amount} 金币"), Args);
    case ECandyFightItemEffect::None:
    default:
        return NSLOCTEXT("CandyFightItems", "MaterialEffect", "任务材料，无法使用");
    }
}
