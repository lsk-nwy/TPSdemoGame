#include "CandyFight/Elements/CandyFightElementTypes.h"

UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Element_Fire, "Element.Fire");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Element_Water, "Element.Water");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Element_Ice, "Element.Ice");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Reaction_Vaporize, "Reaction.Vaporize");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Reaction_Melt, "Reaction.Melt");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Reaction_Freeze, "Reaction.Freeze");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_Reaction_Shatter, "Reaction.Shatter");
UE_DEFINE_GAMEPLAY_TAG(CandyFightTags_State_Frozen, "State.Frozen");

namespace CandyFightElements
{
    bool IsValidElement(ECandyFightElement Element)
    {
        return Element == ECandyFightElement::Fire || Element == ECandyFightElement::Water ||
            Element == ECandyFightElement::Ice;
    }

    FGameplayTag GetElementTag(ECandyFightElement Element)
    {
        switch (Element)
        {
        case ECandyFightElement::Fire: return CandyFightTags_Element_Fire.GetTag();
        case ECandyFightElement::Water: return CandyFightTags_Element_Water.GetTag();
        case ECandyFightElement::Ice: return CandyFightTags_Element_Ice.GetTag();
        default: return FGameplayTag();
        }
    }

    FGameplayTag GetReactionTag(ECandyFightReaction Reaction)
    {
        switch (Reaction)
        {
        case ECandyFightReaction::Vaporize: return CandyFightTags_Reaction_Vaporize.GetTag();
        case ECandyFightReaction::Melt: return CandyFightTags_Reaction_Melt.GetTag();
        case ECandyFightReaction::Freeze: return CandyFightTags_Reaction_Freeze.GetTag();
        case ECandyFightReaction::Shatter: return CandyFightTags_Reaction_Shatter.GetTag();
        default: return FGameplayTag();
        }
    }

    FText GetElementName(ECandyFightElement Element)
    {
        switch (Element)
        {
        case ECandyFightElement::Fire: return NSLOCTEXT("CandyFightElements", "Fire", "火");
        case ECandyFightElement::Water: return NSLOCTEXT("CandyFightElements", "Water", "水");
        case ECandyFightElement::Ice: return NSLOCTEXT("CandyFightElements", "Ice", "冰");
        default: return NSLOCTEXT("CandyFightElements", "None", "物理");
        }
    }

    FText GetReactionName(ECandyFightReaction Reaction)
    {
        switch (Reaction)
        {
        case ECandyFightReaction::Vaporize: return NSLOCTEXT("CandyFightElements", "Vaporize", "蒸发");
        case ECandyFightReaction::Melt: return NSLOCTEXT("CandyFightElements", "Melt", "融化");
        case ECandyFightReaction::Freeze: return NSLOCTEXT("CandyFightElements", "Freeze", "冻结");
        case ECandyFightReaction::Shatter: return NSLOCTEXT("CandyFightElements", "Shatter", "碎冰");
        default: return FText::GetEmpty();
        }
    }

    const TCHAR* GetElementId(ECandyFightElement Element)
    {
        switch (Element)
        {
        case ECandyFightElement::Fire: return TEXT("FIRE");
        case ECandyFightElement::Water: return TEXT("WATER");
        case ECandyFightElement::Ice: return TEXT("ICE");
        default: return TEXT("PHYS");
        }
    }

    const TCHAR* GetReactionId(ECandyFightReaction Reaction)
    {
        switch (Reaction)
        {
        case ECandyFightReaction::Vaporize: return TEXT("VAPORIZE");
        case ECandyFightReaction::Melt: return TEXT("MELT");
        case ECandyFightReaction::Freeze: return TEXT("FREEZE");
        case ECandyFightReaction::Shatter: return TEXT("SHATTER");
        default: return TEXT("");
        }
    }

    FLinearColor GetElementColor(ECandyFightElement Element)
    {
        switch (Element)
        {
        case ECandyFightElement::Fire: return FLinearColor(1.f, .32f, .08f);
        case ECandyFightElement::Water: return FLinearColor(.16f, .48f, 1.f);
        case ECandyFightElement::Ice: return FLinearColor(.55f, .93f, 1.f);
        default: return FLinearColor(.85f, .85f, .85f);
        }
    }

    FLinearColor GetReactionColor(ECandyFightReaction Reaction)
    {
        switch (Reaction)
        {
        case ECandyFightReaction::Vaporize: return FLinearColor(.55f, .95f, 1.f);
        case ECandyFightReaction::Melt: return FLinearColor(1.f, .62f, .2f);
        case ECandyFightReaction::Freeze: return FLinearColor(.55f, .93f, 1.f);
        case ECandyFightReaction::Shatter: return FLinearColor(.85f, .95f, 1.f);
        default: return FLinearColor::White;
        }
    }

    ECandyFightElement GetNextElement(ECandyFightElement Element)
    {
        switch (Element)
        {
        case ECandyFightElement::Fire: return ECandyFightElement::Water;
        case ECandyFightElement::Water: return ECandyFightElement::Ice;
        case ECandyFightElement::Ice: return ECandyFightElement::Fire;
        default: return ECandyFightElement::Fire;
        }
    }

    FString FormatElementShortName(ECandyFightElement Element)
    {
        return FString(GetElementId(Element));
    }
}
