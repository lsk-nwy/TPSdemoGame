#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "CandyFight/Elements/CandyFightElementAuraComponent.h"
#include "CandyFight/Elements/CandyFightElementReactionComponent.h"
#include "CandyFight/Elements/CandyFightFrozenEffectComponent.h"
#include "CandyFight/Elements/CandyFightReactionRules.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
    struct FElementWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        FElementWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        ~FElementWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    };

    bool Nearly(float Actual, float Expected, float Tolerance = .01f)
    {
        return FMath::IsNearlyEqual(Actual, Expected, Tolerance);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAuraDecayTest, "CandyFight.Elements.AuraDecay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAuraDecayTest::RunTest(const FString& Parameters)
{
    // 纯函数：1U 9.5 秒、2U 12 秒（设计文档给定基准）。
    TestTrue(TEXT("1U lasts 9.5s"),
        Nearly(UCandyFightElementAuraComponent::GetDecaySeconds(1.f, 9.5f, 2.5f, 2.f), 9.5f));
    TestTrue(TEXT("2U lasts 12s"),
        Nearly(UCandyFightElementAuraComponent::GetDecaySeconds(2.f, 9.5f, 2.5f, 2.f), 12.f));

    FElementWorld Fixture;
    auto* Target = Fixture.World->SpawnActor<ACandyFightTarget>();
    if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Aura component"), Target->ElementAura.Get())) return false;
    auto* Aura = Target->ElementAura.Get();

    Aura->ApplyAura(ECandyFightElement::Fire, 1.f);
    TestTrue(TEXT("1U applied"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Fire), 1.f));
    TestTrue(TEXT("Element tag present"), Aura->ActiveTags.HasTag(CandyFightTags_Element_Fire.GetTag()));
    Aura->AdvanceTo(4.75);
    TestTrue(TEXT("Half of 1U remains after 4.75s"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Fire), .5f));
    Aura->AdvanceTo(9.6);
    TestFalse(TEXT("1U expired after 9.5s"), Aura->HasAura(ECandyFightElement::Fire));
    TestEqual(TEXT("Aura map cleared"), Aura->Auras.Num(), 0);
    TestTrue(TEXT("Tag removed with aura"), Aura->ActiveTags.IsEmpty());

    Aura->ApplyAura(ECandyFightElement::Water, 2.f);
    Aura->AdvanceTo(15.6);
    TestTrue(TEXT("2U decays at 12s rate"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Water), 1.f));

    Aura->ApplyAura(ECandyFightElement::Water, 1.5f);
    TestTrue(TEXT("Same element stacks and clamps"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Water), 2.f));
    Aura->AdvanceTo(24.);
    TestTrue(TEXT("Refreshed 2U decays from 12s"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Water), .6f));
    Aura->ClearAllAuras();
    TestEqual(TEXT("ClearAllAuras empties the map"), Aura->Auras.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightVaporizeMeltTest, "CandyFight.Elements.VaporizeAndMelt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightVaporizeMeltTest::RunTest(const FString& Parameters)
{
    FElementWorld Fixture;
    auto* Target = Fixture.World->SpawnActor<ACandyFightTarget>();
    if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Reactions"), Target->ElementReactions.Get())) return false;
    auto* Aura = Target->ElementAura.Get();
    auto* Reactions = Target->ElementReactions.Get();

    // 火打水：1.5 倍；1:1 对消，双方都不留附着。
    Aura->ApplyAura(ECandyFightElement::Water, 1.f);
    FCandyFightReactionHit Hit = Reactions->SettleHit(ECandyFightElement::Fire, 1.f, 100.f);
    TestTrue(TEXT("Fire on water vaporizes"), Hit.Reaction == ECandyFightReaction::Vaporize);
    TestTrue(TEXT("Fire on water is 1.5x"), Nearly(Hit.Multiplier, 1.5f));
    TestTrue(TEXT("Damage is multiplied"), Nearly(Hit.FinalDamage, 150.f));
    TestFalse(TEXT("Water aura consumed"), Aura->HasAura(ECandyFightElement::Water));
    TestFalse(TEXT("No fire aura left"), Aura->HasAura(ECandyFightElement::Fire));
    TestTrue(TEXT("Reaction tag registered"),
        Reactions->ActiveReactionTags.HasTag(CandyFightTags_Reaction_Vaporize.GetTag()));
    TestTrue(TEXT("Last reaction recorded"), Reactions->LastReaction == ECandyFightReaction::Vaporize);

    // 水打火：2.0 倍；2U 触发只消耗 1U 附着，剩余 1U 作为新附着。
    Aura->ApplyAura(ECandyFightElement::Fire, 1.f);
    Hit = Reactions->SettleHit(ECandyFightElement::Water, 2.f, 100.f);
    TestTrue(TEXT("Water on fire vaporizes"), Hit.Reaction == ECandyFightReaction::Vaporize);
    TestTrue(TEXT("Water on fire is 2.0x"), Nearly(Hit.Multiplier, 2.f));
    TestTrue(TEXT("Damage is doubled"), Nearly(Hit.FinalDamage, 200.f));
    TestFalse(TEXT("Fire aura consumed"), Aura->HasAura(ECandyFightElement::Fire));
    TestTrue(TEXT("Leftover trigger becomes aura"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Water), 1.f));

    // 火打冰：融化 2.0 倍。
    Aura->ClearAllAuras();
    Aura->ApplyAura(ECandyFightElement::Ice, 1.f);
    Hit = Reactions->SettleHit(ECandyFightElement::Fire, 1.f, 50.f);
    TestTrue(TEXT("Fire on ice melts"), Hit.Reaction == ECandyFightReaction::Melt);
    TestTrue(TEXT("Fire on ice is 2.0x"), Nearly(Hit.Multiplier, 2.f));
    TestTrue(TEXT("Melt damage"), Nearly(Hit.FinalDamage, 100.f));

    // 冰打火：融化 1.5 倍。
    Aura->ClearAllAuras();
    Aura->ApplyAura(ECandyFightElement::Fire, 1.f);
    Hit = Reactions->SettleHit(ECandyFightElement::Ice, 1.f, 50.f);
    TestTrue(TEXT("Ice on fire melts"), Hit.Reaction == ECandyFightReaction::Melt);
    TestTrue(TEXT("Ice on fire is 1.5x"), Nearly(Hit.Multiplier, 1.5f));
    TestTrue(TEXT("Melt damage reversed"), Nearly(Hit.FinalDamage, 75.f));

    // 同元素不反应，只叠加附着并保留伤害。
    Aura->ClearAllAuras();
    Aura->ApplyAura(ECandyFightElement::Fire, 1.f);
    Hit = Reactions->SettleHit(ECandyFightElement::Fire, 1.f, 100.f);
    TestTrue(TEXT("Same element does not react"), Hit.Reaction == ECandyFightReaction::None);
    TestTrue(TEXT("Same element keeps damage"), Nearly(Hit.FinalDamage, 100.f));
    TestTrue(TEXT("Same element stacks to cap"), Nearly(Aura->GetAuraGauge(ECandyFightElement::Fire), 2.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFreezeShatterTest, "CandyFight.Elements.FreezeAndShatter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightFreezeShatterTest::RunTest(const FString& Parameters)
{
    FElementWorld Fixture;
    auto* Target = Fixture.World->SpawnActor<ACandyFightTarget>();
    if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Frozen"), Target->ElementFrozen.Get())) return false;
    auto* Aura = Target->ElementAura.Get();
    auto* Reactions = Target->ElementReactions.Get();
    auto* Frozen = Target->ElementFrozen.Get();

    // 水 + 冰：冻结，不走伤害乘区，冻元素量 = 消耗量 x2。
    Aura->ApplyAura(ECandyFightElement::Water, 1.f);
    FCandyFightReactionHit Hit = Reactions->SettleHit(ECandyFightElement::Ice, 1.f, 100.f);
    TestTrue(TEXT("Water and ice freeze"), Hit.Reaction == ECandyFightReaction::Freeze);
    TestTrue(TEXT("Freeze does not multiply damage"), Nearly(Hit.FinalDamage, 100.f));
    TestTrue(TEXT("Freeze applied flag"), Hit.bFreezeApplied);
    TestTrue(TEXT("Frozen state active"), Frozen->IsFrozen());
    TestTrue(TEXT("State.Frozen tag"), Frozen->StateTags.HasTag(CandyFightTags_State_Frozen.GetTag()));
    TestTrue(TEXT("Frozen gauge is twice consumed"), Nearly(Frozen->FrozenGauge, 2.f));
    TestFalse(TEXT("Aura cleared by freeze"), Aura->HasAura(ECandyFightElement::Water));

    // 冻结中的冰/水不产生新反应。
    Hit = Reactions->SettleHit(ECandyFightElement::Ice, 1.f, 100.f);
    TestTrue(TEXT("Ice on frozen is inert"), Hit.Reaction == ECandyFightReaction::None);
    TestTrue(TEXT("Still frozen"), Frozen->IsFrozen());

    // 火打冻结：按冰附着走融化 2.0 并立即解冻。
    Hit = Reactions->SettleHit(ECandyFightElement::Fire, 1.f, 100.f);
    TestTrue(TEXT("Fire melts frozen target"), Hit.Reaction == ECandyFightReaction::Melt);
    TestTrue(TEXT("Frozen melt is 2.0x"), Nearly(Hit.Multiplier, 2.f));
    TestTrue(TEXT("Frozen melt damage"), Nearly(Hit.FinalDamage, 200.f));
    TestFalse(TEXT("Fire unfreezes"), Frozen->IsFrozen());
    TestTrue(TEXT("Frozen tag removed"), Frozen->StateTags.IsEmpty());

    // 物理累计 30 点破冰：20 + 20 的第二次触发碎冰。
    Aura->ApplyAura(ECandyFightElement::Water, 1.f);
    Reactions->SettleHit(ECandyFightElement::Ice, 1.f, 10.f);
    TestTrue(TEXT("Freeze again"), Frozen->IsFrozen());
    Hit = Reactions->SettleHit(ECandyFightElement::None, 1.f, 20.f);
    TestTrue(TEXT("Below break threshold"), Frozen->IsFrozen() && Hit.Reaction == ECandyFightReaction::None);
    Hit = Reactions->SettleHit(ECandyFightElement::None, 1.f, 20.f);
    TestTrue(TEXT("Shatter reaction"), Hit.Reaction == ECandyFightReaction::Shatter);
    TestFalse(TEXT("Shatter unfreezes"), Frozen->IsFrozen());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFreezeExpiryTest, "CandyFight.Elements.FreezeMovementAndExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightFreezeExpiryTest::RunTest(const FString& Parameters)
{
    FElementWorld Fixture;
    auto* Enemy = Fixture.World->SpawnActor<ACandyFightEnemyCharacter>();
    if (!TestNotNull(TEXT("Enemy"), Enemy)) return false;
    Enemy->DispatchBeginPlay();
    if (!TestNotNull(TEXT("Aura"), Enemy->ElementAura.Get()) ||
        !TestNotNull(TEXT("Reactions"), Enemy->ElementReactions.Get()) ||
        !TestNotNull(TEXT("Frozen"), Enemy->ElementFrozen.Get())) return false;
    auto* Aura = Enemy->ElementAura.Get();
    auto* Reactions = Enemy->ElementReactions.Get();
    auto* Frozen = Enemy->ElementFrozen.Get();
    auto* Movement = Enemy->GetCharacterMovement();

    Aura->ApplyAura(ECandyFightElement::Water, 1.f);
    Reactions->SettleHit(ECandyFightElement::Ice, 1.f, 5.f);
    TestTrue(TEXT("Enemy frozen"), Frozen->IsFrozen());
    TestTrue(TEXT("Movement disabled while frozen"), Movement->MovementMode == MOVE_None);
    TestTrue(TEXT("Freeze label visible text"), Frozen->GetStateLabelText().Contains(TEXT("FROZEN")));
    TestFalse(TEXT("Frozen enemy cannot shoot"), Enemy->IsCombatShotAllowed());

    Frozen->AdvanceTo(2.1);
    TestFalse(TEXT("Freeze expires after 2s"), Frozen->IsFrozen());
    TestTrue(TEXT("Movement restored"), Movement->MovementMode != MOVE_None);
    TestTrue(TEXT("State label cleared"), Frozen->GetStateLabelText().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightElementSwitchTest, "CandyFight.Elements.WeaponElementSwitch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightElementSwitchTest::RunTest(const FString& Parameters)
{
    FElementWorld Fixture;
    auto* Character = Fixture.World->SpawnActor<ACandyFightCharacter>();
    if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Combat"), Character->Combat.Get())) return false;
    auto* Combat = Character->Combat.Get();

    TestTrue(TEXT("Local player starts with fire"), Combat->GetElement() == ECandyFightElement::Fire);
    TestTrue(TEXT("Set water"), Combat->SetElement(ECandyFightElement::Water));
    TestTrue(TEXT("Water active"), Combat->GetElement() == ECandyFightElement::Water);
    Combat->CycleElement();
    TestTrue(TEXT("Cycle to ice"), Combat->GetElement() == ECandyFightElement::Ice);
    Combat->CycleElement();
    TestTrue(TEXT("Cycle to fire"), Combat->GetElement() == ECandyFightElement::Fire);
    TestFalse(TEXT("Physics is not a selectable element"), Combat->SetElement(ECandyFightElement::None));

    auto* HUD = NewObject<UCandyFightWeaponHUD>();
    HUD->Initialize();
    HUD->TakeWidget();
    HUD->Connect(Combat);
    if (TestNotNull(TEXT("HUD status text"), HUD->StatusText.Get()))
    {
        TestTrue(TEXT("HUD hints the Q key"), HUD->StatusText->GetText().ToString().Contains(TEXT("Q")));
        Combat->SetElement(ECandyFightElement::Water);
        TestTrue(TEXT("HUD shows current element"), HUD->StatusText->GetText().ToString().Contains(TEXT("水")));
        Combat->SetElement(ECandyFightElement::Fire);
        TestTrue(TEXT("HUD refreshes on switch"), HUD->StatusText->GetText().ToString().Contains(TEXT("火")));
    }
    HUD->Connect(nullptr);
    HUD->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightElementProjectileTest, "CandyFight.Elements.ProjectileCarriesElement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightElementProjectileTest::RunTest(const FString& Parameters)
{
    FElementWorld Fixture;
    auto* Shooter = Fixture.World->SpawnActor<AActor>();
    auto* Character = Fixture.World->SpawnActor<ACandyFightCharacter>();
    auto* Target = Fixture.World->SpawnActor<ACandyFightTarget>(FVector(1000., 0., 90.), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Target"), Target)) return false;
    auto* Combat = Character->Combat.Get();
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    Config->Damage = 20.f;

    Target->ElementAura->ApplyAura(ECandyFightElement::Water, 1.f);
    Combat->SetElement(ECandyFightElement::Fire);
    auto* Shot = Fixture.World->SpawnActor<ACandyFightProjectileShot>();
    TArray<FVector> Directions;
    Directions.Add(FVector::ForwardVector);
    Shot->Initialize(*Config, FVector(0., 0., 90.), Directions, Shooter, nullptr, Combat, nullptr, 0.);
    Shot->AdvanceTo(.3);
    TestTrue(TEXT("Shot carries current element"), Nearly(Target->Health, 170.f));
    TestTrue(TEXT("Shot damage triggers vaporize"), Target->ElementReactions->LastReaction == ECandyFightReaction::Vaporize);

    // 切到冰后同一把枪的下一发改了元素：火附着 + 冰 = 融化。
    Target->Health = 200.f;
    Target->ElementAura->ClearAllAuras();
    Target->ElementAura->ApplyAura(ECandyFightElement::Fire, 1.f);
    Combat->SetElement(ECandyFightElement::Ice);
    Shot = Fixture.World->SpawnActor<ACandyFightProjectileShot>();
    Shot->Initialize(*Config, FVector(0., 0., 90.), Directions, Shooter, nullptr, Combat, nullptr, 0.);
    Shot->AdvanceTo(.3);
    TestTrue(TEXT("Ice shot melts"), Target->ElementReactions->LastReaction == ECandyFightReaction::Melt);
    // 冰打火是 1.5 倍：20 * 1.5 = 30。
    TestTrue(TEXT("Melt multiplies damage"), Nearly(Target->Health, 170.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightElementRuleOverrideTest, "CandyFight.Elements.RuleAssetOverride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightElementRuleOverrideTest::RunTest(const FString& Parameters)
{
    FElementWorld Fixture;
    auto* Target = Fixture.World->SpawnActor<ACandyFightTarget>();
    if (!TestNotNull(TEXT("Target"), Target)) return false;
    auto* Aura = Target->ElementAura.Get();
    auto* Reactions = Target->ElementReactions.Get();

    // 默认规则：水打火 2.0。
    Aura->ApplyAura(ECandyFightElement::Fire, 1.f);
    FCandyFightReactionHit Hit = Reactions->SettleHit(ECandyFightElement::Water, 1.f, 100.f);
    TestTrue(TEXT("Default rule doubles"), Nearly(Hit.FinalDamage, 200.f));

    // 数据驱动：把规则资产里的倍率改成 3.0，行为立即变化。
    auto* Rules = NewObject<UCandyFightReactionRules>();
    for (FCandyFightReactionRule& Rule : Rules->Rules)
    {
        if (Rule.Reaction == ECandyFightReaction::Vaporize) Rule.MultiplierBTriggersA = 3.f;
    }
    Reactions->Rules = Rules;
    Aura->ApplyAura(ECandyFightElement::Fire, 1.f);
    Hit = Reactions->SettleHit(ECandyFightElement::Water, 1.f, 100.f);
    TestTrue(TEXT("Rule asset overrides multiplier"), Nearly(Hit.FinalDamage, 300.f));
    return true;
}
#endif
