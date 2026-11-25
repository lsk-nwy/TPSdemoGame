#include "CandyFight/Elements/CandyFightReactionRules.h"

FCandyFightReactionRule FCandyFightReactionRule::MakeVaporize()
{
    FCandyFightReactionRule Rule;
    Rule.ElementA = ECandyFightElement::Fire;
    Rule.ElementB = ECandyFightElement::Water;
    Rule.Reaction = ECandyFightReaction::Vaporize;
    Rule.MultiplierATriggersB = 1.5f; // 火打水：1.5
    Rule.MultiplierBTriggersA = 2.f;  // 水打火：2.0
    Rule.AuraCostPerGauge = 1.f;
    Rule.TriggerCostPerAura = 1.f;
    return Rule;
}

FCandyFightReactionRule FCandyFightReactionRule::MakeMelt()
{
    FCandyFightReactionRule Rule;
    Rule.ElementA = ECandyFightElement::Fire;
    Rule.ElementB = ECandyFightElement::Ice;
    Rule.Reaction = ECandyFightReaction::Melt;
    Rule.MultiplierATriggersB = 2.f;  // 火打冰：2.0
    Rule.MultiplierBTriggersA = 1.5f; // 冰打火：1.5
    Rule.AuraCostPerGauge = 1.f;
    Rule.TriggerCostPerAura = 1.f;
    return Rule;
}

FCandyFightReactionRule FCandyFightReactionRule::MakeFreeze()
{
    FCandyFightReactionRule Rule;
    Rule.ElementA = ECandyFightElement::Water;
    Rule.ElementB = ECandyFightElement::Ice;
    Rule.Reaction = ECandyFightReaction::Freeze;
    Rule.MultiplierATriggersB = 1.f;
    Rule.MultiplierBTriggersA = 1.f;
    Rule.bConsumeAllAura = true;
    Rule.FreezeSeconds = 2.f;
    Rule.FreezeGaugeMultiplier = 2.f;
    return Rule;
}

bool FCandyFightReactionRule::Matches(ECandyFightElement First, ECandyFightElement Second) const
{
    return (ElementA == First && ElementB == Second) || (ElementA == Second && ElementB == First);
}

float FCandyFightReactionRule::GetMultiplier(ECandyFightElement AuraElement, ECandyFightElement TriggerElement) const
{
    // MultiplierATriggersB：A 是触发元素、B 是已有附着。
    if (AuraElement == ElementB && TriggerElement == ElementA) return MultiplierATriggersB;
    // MultiplierBTriggersA：B 是触发元素、A 是已有附着。
    if (AuraElement == ElementA && TriggerElement == ElementB) return MultiplierBTriggersA;
    return 1.f;
}

UCandyFightReactionRules::UCandyFightReactionRules()
{
    Rules.Add(FCandyFightReactionRule::MakeVaporize());
    Rules.Add(FCandyFightReactionRule::MakeMelt());
    Rules.Add(FCandyFightReactionRule::MakeFreeze());
}

const FCandyFightReactionRule* UCandyFightReactionRules::FindRule(ECandyFightElement First, ECandyFightElement Second) const
{
    if (First == ECandyFightElement::None || Second == ECandyFightElement::None || First == Second) return nullptr;
    for (const FCandyFightReactionRule& Rule : Rules)
    {
        if (Rule.Matches(First, Second)) return &Rule;
    }
    return nullptr;
}

const FCandyFightReactionRule* UCandyFightReactionRules::FindRule(ECandyFightReaction Reaction) const
{
    for (const FCandyFightReactionRule& Rule : Rules)
    {
        if (Rule.Reaction == Reaction) return &Rule;
    }
    return nullptr;
}

const UCandyFightReactionRules& UCandyFightReactionRules::GetDefaultRules()
{
    return *GetDefault<UCandyFightReactionRules>();
}
