#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Elements/CandyFightElementAuraComponent.h"
#include "CandyFight/Elements/CandyFightElementReactionComponent.h"
#include "CandyFight/Elements/CandyFightFrozenEffectComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ACandyFightTarget::ACandyFightTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
    SetRootComponent(HitBox);
    HitBox->SetBoxExtent(FVector(40., 40., 90.));
    HitBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    HitBox->SetCollisionResponseToChannel(CandyFightWeapons::TraceChannel, ECR_Block);
    HitBox->SetGenerateOverlapEvents(false);
    TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    TargetMesh->SetupAttachment(HitBox);
    TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TargetMesh->SetRelativeScale3D(FVector(.8, .8, 1.8));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    TargetMesh->SetStaticMesh(Cube.Object);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(HitBox);
    Label->SetRelativeLocation(FVector(0., 0., 120.));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(25.f);
    Label->SetText(FText::FromString(TEXT("200 HP")));
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ElementAura = CreateDefaultSubobject<UCandyFightElementAuraComponent>(TEXT("ElementAura"));
    ElementReactions = CreateDefaultSubobject<UCandyFightElementReactionComponent>(TEXT("ElementReactions"));
    ElementFrozen = CreateDefaultSubobject<UCandyFightFrozenEffectComponent>(TEXT("ElementFrozen"));
}
void ACandyFightTarget::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
    Material = TargetMesh->CreateDynamicMaterialInstance(0);
    if (Material) Material->SetVectorParameterValue(TEXT("Color"), TargetColor);
    UpdateLabel(false);
}
void ACandyFightTarget::UpdateLabel(bool bShowDamage)
{
    bShowingDamage = bShowDamage;
    Label->SetText(FText::FromString(bShowDamage ? FString::Printf(TEXT("%.0f HP\n-%.0f"), Health, LastDamage) : FString::Printf(TEXT("%.0f HP"), Health)));
}
float ACandyFightTarget::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* InstigatorController, AActor* DamageCauser)
{
    if (Health <= 0.f || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f) return 0.f;

    const FCandyFightProjectileDamageEvent* ProjectileEvent = DamageEvent.IsOfType(FCandyFightProjectileDamageEvent::ClassID)
        ? static_cast<const FCandyFightProjectileDamageEvent*>(&DamageEvent) : nullptr;
    FCandyFightReactionHit ReactionHit;
    if (ElementReactions)
    {
        ReactionHit = ElementReactions->SettleHit(
            ProjectileEvent ? ProjectileEvent->Element : ECandyFightElement::None,
            ProjectileEvent ? ProjectileEvent->ElementGauge : 1.f, DamageAmount);
        DamageAmount = ReactionHit.FinalDamage;
        if (DamageAmount <= 0.f) return 0.f;
    }

    bool bUpdateDamageText = true;
    float DisplayDamage = DamageAmount;
    if (ProjectileEvent)
    {
        bUpdateDamageText = ProjectileEvent->ShotId >= DisplayedShotId;
        if (bUpdateDamageText)
        {
            DisplayedShotId = ProjectileEvent->ShotId;
            // 反应倍率已经计入本次伤害；无反应时保持整发跳字累计。
            DisplayDamage = ReactionHit.HasReaction() ? DamageAmount : ProjectileEvent->CumulativeDamage;
        }
    }
    if (bUpdateDamageText)
    {
        LastDamage = DisplayDamage;
        DamageTextUntil = GetWorld()->GetTimeSeconds() + .8;
    }
    ++HitCount;
    Health = FMath::Max(0.f, Health - DamageAmount);
    FlashUntil = GetWorld()->GetTimeSeconds() + .10;
    if (Material) Material->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
    UpdateLabel(bUpdateDamageText || bShowingDamage);
    if (Health <= 0.f)
    {
        if (ElementFrozen) ElementFrozen->Unfreeze(false);
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        RespawnAt = GetWorld()->GetTimeSeconds() + RespawnSeconds;
    }
    return DamageAmount;
}
void ACandyFightTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now = GetWorld()->GetTimeSeconds();
    if (Health <= 0.f && Now >= RespawnAt)
    {
        Health = MaxHealth;
        DisplayedShotId = 0;
        if (ElementAura) ElementAura->ClearAllAuras();
        if (ElementFrozen) ElementFrozen->Unfreeze(false);
        SetActorHiddenInGame(false);
        SetActorEnableCollision(true);
        UpdateLabel(false);
    }
    if (FlashUntil > 0. && Now >= FlashUntil)
    {
        if (Material) Material->SetVectorParameterValue(TEXT("Color"), TargetColor);
        FlashUntil = 0.;
    }
    if (bShowingDamage && Now >= DamageTextUntil) UpdateLabel(false);
    if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
        Label->SetWorldRotation((Camera->GetCameraLocation() - Label->GetComponentLocation()).Rotation());
}
