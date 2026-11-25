#include "CandyFight/Elements/CandyFightElementReactionComponent.h"
#include "CandyFight/Elements/CandyFightElementAuraComponent.h"
#include "CandyFight/Elements/CandyFightFrozenEffectComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

UCandyFightElementReactionComponent::UCandyFightElementReactionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCandyFightElementReactionComponent::BeginPlay()
{
    Super::BeginPlay();
    ResolveRefs();
}

void UCandyFightElementReactionComponent::ResolveRefs() const
{
    const AActor* Owner = GetOwner();
    if (!Owner) return;
    if (!Aura.IsValid()) Aura = Owner->FindComponentByClass<UCandyFightElementAuraComponent>();
    if (!Frozen.IsValid()) Frozen = Owner->FindComponentByClass<UCandyFightFrozenEffectComponent>();
}

UCandyFightElementAuraComponent* UCandyFightElementReactionComponent::GetAuraComponent() const
{
    ResolveRefs();
    return Aura.Get();
}

UCandyFightFrozenEffectComponent* UCandyFightElementReactionComponent::GetFrozenComponent() const
{
    ResolveRefs();
    return Frozen.Get();
}

FCandyFightReactionHit UCandyFightElementReactionComponent::SettleHit(ECandyFightElement IncomingElement,
    float IncomingGauge, float BaseDamage)
{
    FCandyFightReactionHit Hit;
    Hit.Element = IncomingElement;
    Hit.BaseDamage = FMath::IsFinite(BaseDamage) ? FMath::Max(0.f, BaseDamage) : 0.f;
    Hit.Multiplier = 1.f;
    Hit.FinalDamage = Hit.BaseDamage;
    if (Hit.BaseDamage <= 0.f) return Hit;

    const float Gauge = FMath::IsFinite(IncomingGauge) && IncomingGauge > 0.f ? IncomingGauge : 1.f;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;

    UCandyFightElementAuraComponent* AuraComponent = GetAuraComponent();
    UCandyFightFrozenEffectComponent* FrozenComponent = GetFrozenComponent();
    const UCandyFightReactionRules& RulesRef = Rules ? *Rules.Get() : UCandyFightReactionRules::GetDefaultRules();

    // 冻结中：火 -> 融化并立即解冻；物理 -> 累计破冰；冰/水不再产生新附着。
    if (FrozenComponent && FrozenComponent->IsFrozen())
    {
        if (IncomingElement == ECandyFightElement::Fire)
        {
            const FCandyFightReactionRule* Rule = RulesRef.FindRule(ECandyFightElement::Fire, ECandyFightElement::Ice);
            Hit.Reaction = ECandyFightReaction::Melt;
            Hit.Multiplier = Rule ? Rule->GetMultiplier(ECandyFightElement::Ice, ECandyFightElement::Fire) : 2.f;
            Hit.BonusDamage = Rule ? Rule->FlatDamageBonus : 0.f;
            if (AuraComponent) AuraComponent->ClearAllAuras();
            FrozenComponent->Unfreeze(true);
        }
        else if (IncomingElement == ECandyFightElement::None)
        {
            if (FrozenComponent->NotifyDamage(Hit.BaseDamage)) Hit.Reaction = ECandyFightReaction::Shatter;
        }
        FinalizeHit(Hit, Now);
        return Hit;
    }

    const ECandyFightElement AuraElement = AuraComponent ? AuraComponent->GetStrongestAura() : ECandyFightElement::None;
    if (AuraComponent && AuraElement != ECandyFightElement::None && AuraElement != IncomingElement)
    {
        const FCandyFightReactionRule* Rule = RulesRef.FindRule(AuraElement, IncomingElement);
        if (Rule && Rule->Reaction == ECandyFightReaction::Freeze && FrozenComponent)
        {
            const float AuraGauge = AuraComponent->GetAuraGauge(AuraElement);
            const float Consumed = Rule->bConsumeAllAura ? AuraGauge : FMath::Min(AuraGauge, Gauge);
            AuraComponent->ClearAllAuras();
            Hit.Reaction = ECandyFightReaction::Freeze;
            Hit.AuraConsumed = Consumed;
            Hit.Multiplier = 1.f;
            Hit.BonusDamage = Rule->FlatDamageBonus;
            Hit.bFreezeApplied = FrozenComponent->Freeze(Rule->FreezeSeconds, Consumed * Rule->FreezeGaugeMultiplier);
        }
        else if (Rule && Rule->Reaction != ECandyFightReaction::Freeze && Rule->Reaction != ECandyFightReaction::None)
        {
            const float AuraGauge = AuraComponent->GetAuraGauge(AuraElement);
            const float Consumed = Rule->bConsumeAllAura
                ? AuraGauge
                : FMath::Min(AuraGauge, Gauge * FMath::Max(0.f, Rule->AuraCostPerGauge));
            const float TriggerConsumed = FMath::Min(Gauge, Consumed * FMath::Max(0.f, Rule->TriggerCostPerAura));
            Hit.Reaction = Rule->Reaction;
            Hit.Multiplier = Rule->GetMultiplier(AuraElement, IncomingElement);
            Hit.BonusDamage = Rule->FlatDamageBonus;
            Hit.AuraConsumed = AuraComponent->ConsumeAura(AuraElement, Consumed);
            // 触发元素若还有剩余，就作为新的附着留下来。
            const float Leftover = Gauge - TriggerConsumed;
            if (Leftover > KINDA_SMALL_NUMBER) AuraComponent->ApplyAura(IncomingElement, Leftover);
        }
    }

    if (Hit.Reaction == ECandyFightReaction::None && AuraComponent && IncomingElement != ECandyFightElement::None)
        AuraComponent->ApplyAura(IncomingElement, Gauge);

    FinalizeHit(Hit, Now);
    return Hit;
}

void UCandyFightElementReactionComponent::FinalizeHit(FCandyFightReactionHit& Hit, double Now)
{
    Hit.FinalDamage = FMath::RoundToFloat(FMath::Max(0.f, Hit.BaseDamage * Hit.Multiplier + Hit.BonusDamage));
    if (Hit.Reaction == ECandyFightReaction::None) return;
    LastReaction = Hit.Reaction;
    ReactionTagUntil = Now + FMath::Max(0.f, ReactionTagSeconds);
    ActiveReactionTags.Reset();
    ActiveReactionTags.AddTag(CandyFightElements::GetReactionTag(Hit.Reaction));
    ShowReactionLabel(Hit, Now);
    OnReaction.Broadcast(Hit);
}

void UCandyFightElementReactionComponent::ShowReactionLabel(const FCandyFightReactionHit& Hit, double Now)
{
    if (!bShowReactionLabel) return;
    EnsureLabel();
    ReactionLabelUntil = Now + FMath::Max(.01f, ReactionLabelSeconds);
    if (Hit.Multiplier > 1.0001f)
        ReactionLabelText = FString::Printf(TEXT("%s x%.1f"), CandyFightElements::GetReactionId(Hit.Reaction), Hit.Multiplier);
    else
        ReactionLabelText = FString(CandyFightElements::GetReactionId(Hit.Reaction));
    if (ReactionLabel)
    {
        ReactionLabel->SetText(FText::FromString(ReactionLabelText));
        ReactionLabel->SetTextRenderColor(CandyFightElements::GetReactionColor(Hit.Reaction).ToFColor(true));
        ReactionLabel->SetVisibility(true);
    }
}

void UCandyFightElementReactionComponent::AdvanceTo(double Now)
{
    if (!FMath::IsFinite(Now)) return;
    if (!ActiveReactionTags.IsEmpty() && Now >= ReactionTagUntil) ActiveReactionTags.Reset();
    UpdateLabelVisibility(Now);
}

void UCandyFightElementReactionComponent::UpdateLabelVisibility(double Now)
{
    if (ReactionLabel && ReactionLabel->IsVisible() && Now >= ReactionLabelUntil)
        ReactionLabel->SetVisibility(false);
}

void UCandyFightElementReactionComponent::EnsureLabel()
{
    if (ReactionLabel || !bShowReactionLabel) return;
    AActor* Owner = GetOwner();
    if (!Owner) return;
    const FName LabelName = MakeUniqueObjectName(Owner, UTextRenderComponent::StaticClass(), TEXT("CandyFightReactionLabel"));
    ReactionLabel = NewObject<UTextRenderComponent>(Owner, LabelName);
    if (!ReactionLabel) return;
    ReactionLabel->RegisterComponent();
    if (USceneComponent* Root = Owner->GetRootComponent())
        ReactionLabel->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
    ReactionLabel->SetRelativeLocation(FVector(0., 0., FMath::Max(1.f, ReactionLabelHeight)));
    ReactionLabel->SetHorizontalAlignment(EHTA_Center);
    ReactionLabel->SetWorldSize(FMath::Max(1.f, ReactionLabelSize));
    ReactionLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ReactionLabel->SetGenerateOverlapEvents(false);
    ReactionLabel->SetCanEverAffectNavigation(false);
    ReactionLabel->SetVisibility(false);
}

void UCandyFightElementReactionComponent::FaceCamera()
{
    if (!ReactionLabel || !ReactionLabel->IsVisible()) return;
    if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const FVector ToCamera = Camera->GetCameraLocation() - ReactionLabel->GetComponentLocation();
        if (!ToCamera.IsNearlyZero()) ReactionLabel->SetWorldRotation(ToCamera.Rotation());
    }
}

void UCandyFightElementReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (const UWorld* World = GetWorld()) AdvanceTo(World->GetTimeSeconds());
    FaceCamera();
}
