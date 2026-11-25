#include "CandyFight/Elements/CandyFightElementAuraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

UCandyFightElementAuraComponent::UCandyFightElementAuraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    // 硬引用引擎基础球体/材质，保证打包时被 cook 并用于头顶元素光球。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Sphere.Succeeded()) AuraOrbMesh = Sphere.Object;
    if (BasicMaterial.Succeeded()) AuraOrbMaterial = BasicMaterial.Object;
}

void UCandyFightElementAuraComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const UWorld* World = GetWorld()) LastAdvanceTime = World->GetTimeSeconds();
}

float UCandyFightElementAuraComponent::GetDecaySeconds(float InitialGauge, float SecondsPerGauge,
    float AdditionalSecondsPerGauge, float MaxAuraGauge)
{
    if (!FMath::IsFinite(InitialGauge) || InitialGauge <= 0.f) return 0.f;
    const float Cap = FMath::IsFinite(MaxAuraGauge) ? FMath::Max(0.1f, MaxAuraGauge) : 2.f;
    const float Gauge = FMath::Min(InitialGauge, Cap);
    const float Base = FMath::IsFinite(SecondsPerGauge) && SecondsPerGauge > 0.f ? SecondsPerGauge : 9.5f;
    const float Extra = FMath::IsFinite(AdditionalSecondsPerGauge) && AdditionalSecondsPerGauge >= 0.f
        ? AdditionalSecondsPerGauge : 0.f;
    return Gauge < 1.f ? Gauge * Base : Base + (Gauge - 1.f) * Extra;
}

void UCandyFightElementAuraComponent::ApplyAura(ECandyFightElement Element, float Gauge)
{
    if (!CandyFightElements::IsValidElement(Element) || !FMath::IsFinite(Gauge) || Gauge <= 0.f) return;
    const float Cap = FMath::IsFinite(MaxAuraGauge) ? FMath::Max(0.1f, MaxAuraGauge) : 2.f;
    FCandyFightAuraEntry& Entry = Auras.FindOrAdd(Element);
    Entry.Element = Element;
    Entry.Gauge = FMath::Min(Cap, Entry.Gauge + Gauge);
    const float DecaySeconds = GetDecaySeconds(Entry.Gauge, SecondsPerGauge, AdditionalSecondsPerGauge, Cap);
    Entry.DecayPerSecond = DecaySeconds > 0.f ? Entry.Gauge / DecaySeconds : 0.f;
    RefreshTags();
    UpdateLabel();
    OnAuraChanged.Broadcast(Element, Entry.Gauge, Cap);
}

float UCandyFightElementAuraComponent::ConsumeAura(ECandyFightElement Element, float Amount)
{
    if (!CandyFightElements::IsValidElement(Element) || !FMath::IsFinite(Amount) || Amount <= 0.f) return 0.f;
    FCandyFightAuraEntry* Entry = Auras.Find(Element);
    if (!Entry || Entry->Gauge <= 0.f) return 0.f;
    const float Consumed = FMath::Min(Entry->Gauge, Amount);
    Entry->Gauge = FMath::Max(0.f, Entry->Gauge - Consumed);
    if (Entry->Gauge <= KINDA_SMALL_NUMBER)
    {
        RemoveAuraEntry(Element, false);
    }
    else
    {
        RefreshTags();
        UpdateLabel();
        OnAuraChanged.Broadcast(Element, Entry->Gauge, MaxAuraGauge);
    }
    return Consumed;
}

void UCandyFightElementAuraComponent::ClearAura(ECandyFightElement Element)
{
    if (!Auras.Contains(Element)) return;
    RemoveAuraEntry(Element, false);
}

void UCandyFightElementAuraComponent::ClearAllAuras()
{
    if (Auras.IsEmpty()) return;
    TArray<ECandyFightElement> Elements;
    Auras.GetKeys(Elements);
    for (const ECandyFightElement Element : Elements) RemoveAuraEntry(Element, false);
}

float UCandyFightElementAuraComponent::GetAuraGauge(ECandyFightElement Element) const
{
    const FCandyFightAuraEntry* Entry = Auras.Find(Element);
    return Entry ? FMath::Max(0.f, Entry->Gauge) : 0.f;
}

bool UCandyFightElementAuraComponent::HasAura(ECandyFightElement Element) const
{
    return GetAuraGauge(Element) > KINDA_SMALL_NUMBER;
}

ECandyFightElement UCandyFightElementAuraComponent::GetStrongestAura() const
{
    ECandyFightElement Strongest = ECandyFightElement::None;
    float BestGauge = KINDA_SMALL_NUMBER;
    // 固定顺序保证同量时的结果确定，避免依赖 TMap 迭代顺序。
    for (const ECandyFightElement Element : {ECandyFightElement::Fire, ECandyFightElement::Water, ECandyFightElement::Ice})
    {
        const float Gauge = GetAuraGauge(Element);
        if (Gauge > BestGauge)
        {
            BestGauge = Gauge;
            Strongest = Element;
        }
    }
    return Strongest;
}

FCandyFightAuraEntry UCandyFightElementAuraComponent::GetAura(ECandyFightElement Element) const
{
    if (const FCandyFightAuraEntry* Entry = Auras.Find(Element)) return *Entry;
    return FCandyFightAuraEntry();
}

void UCandyFightElementAuraComponent::AdvanceTo(double Now)
{
    if (!FMath::IsFinite(Now)) return;
    if (Now < LastAdvanceTime)
    {
        // 世界时间回退（重开/暂停恢复）：只重新基准，不倒扣。
        LastAdvanceTime = Now;
        return;
    }
    const float DeltaTime = float(Now - LastAdvanceTime);
    if (DeltaTime <= 0.f) return;
    LastAdvanceTime = Now;
    if (Auras.IsEmpty()) return;

    TArray<ECandyFightElement> Expired;
    for (TPair<ECandyFightElement, FCandyFightAuraEntry>& Pair : Auras)
    {
        Pair.Value.Gauge = FMath::Max(0.f, Pair.Value.Gauge - Pair.Value.DecayPerSecond * DeltaTime);
        if (Pair.Value.Gauge <= KINDA_SMALL_NUMBER) Expired.Add(Pair.Key);
    }
    for (const ECandyFightElement Element : Expired) RemoveAuraEntry(Element, true);
}

void UCandyFightElementAuraComponent::RemoveAuraEntry(ECandyFightElement Element, bool bExpired)
{
    if (Auras.Remove(Element) <= 0) return;
    RefreshTags();
    UpdateLabel();
    OnAuraChanged.Broadcast(Element, 0.f, MaxAuraGauge);
    if (bExpired) OnAuraExpired.Broadcast(Element);
}

void UCandyFightElementAuraComponent::RefreshTags()
{
    ActiveTags.Reset();
    for (const ECandyFightElement Element : {ECandyFightElement::Fire, ECandyFightElement::Water, ECandyFightElement::Ice})
    {
        if (HasAura(Element)) ActiveTags.AddTag(CandyFightElements::GetElementTag(Element));
    }
}

void UCandyFightElementAuraComponent::AdvanceToNow()
{
    if (const UWorld* World = GetWorld()) AdvanceTo(World->GetTimeSeconds());
}

void UCandyFightElementAuraComponent::EnsureLabel()
{
    if (AuraLabel || !bShowAuraLabel) return;
    AActor* Owner = GetOwner();
    if (!Owner) return;
    const FName LabelName = MakeUniqueObjectName(Owner, UTextRenderComponent::StaticClass(), TEXT("CandyFightAuraLabel"));
    AuraLabel = NewObject<UTextRenderComponent>(Owner, LabelName);
    if (!AuraLabel) return;
    AuraLabel->RegisterComponent();
    if (USceneComponent* Root = Owner->GetRootComponent())
        AuraLabel->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
    AuraLabel->SetRelativeLocation(FVector(0., 0., FMath::Max(1.f, AuraLabelHeight)));
    AuraLabel->SetHorizontalAlignment(EHTA_Center);
    AuraLabel->SetWorldSize(FMath::Max(1.f, AuraLabelSize));
    AuraLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AuraLabel->SetGenerateOverlapEvents(false);
    AuraLabel->SetCanEverAffectNavigation(false);
    AuraLabel->SetVisibility(false);
}

void UCandyFightElementAuraComponent::UpdateLabel()
{
    if (bShowAuraLabel)
    {
        int32 Count = 0;
        TArray<FString> Parts;
        FLinearColor Color = FLinearColor(.85f, .85f, .85f);
        for (const ECandyFightElement Element : {ECandyFightElement::Fire, ECandyFightElement::Water, ECandyFightElement::Ice})
        {
            const float Gauge = GetAuraGauge(Element);
            if (Gauge <= KINDA_SMALL_NUMBER) continue;
            Parts.Add(FString::Printf(TEXT("%s %.1fU"), CandyFightElements::GetElementId(Element), Gauge));
            Color = CandyFightElements::GetElementColor(Element);
            ++Count;
        }
        const FString Text = FString::Join(Parts, TEXT("  "));
        if (Count == 0)
        {
            if (AuraLabel) AuraLabel->SetVisibility(false);
        }
        else
        {
            EnsureLabel();
            if (AuraLabel)
            {
                if (Text != AuraLabelText) AuraLabel->SetText(FText::FromString(Text));
                AuraLabel->SetTextRenderColor(Color.ToFColor(true));
                AuraLabel->SetRelativeLocation(FVector(0., 0., FMath::Max(1.f, AuraLabelHeight)));
                AuraLabel->SetVisibility(true);
            }
        }
        AuraLabelText = Text;
    }
    UpdateOrbs();
}

void UCandyFightElementAuraComponent::EnsureOrbs()
{
    if (AuraOrbMesh == nullptr || AuraOrbMaterial == nullptr) return;
    AActor* Owner = GetOwner();
    if (!Owner) return;
    while (AuraOrbs.Num() < 3)
    {
        const FName OrbName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("CandyFightAuraOrb"));
        UStaticMeshComponent* Orb = NewObject<UStaticMeshComponent>(Owner, OrbName);
        if (!Orb) break;
        Orb->SetMobility(EComponentMobility::Movable);
        Orb->RegisterComponent();
        if (USceneComponent* Root = Owner->GetRootComponent())
            Orb->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
        Orb->SetStaticMesh(AuraOrbMesh);
        Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Orb->SetGenerateOverlapEvents(false);
        Orb->SetCanEverAffectNavigation(false);
        Orb->SetCastShadow(false);
        Orb->SetVisibility(false);
        UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(AuraOrbMaterial, this);
        if (Instance) Orb->SetMaterial(0, Instance);
        AuraOrbs.Add(Orb);
        AuraOrbColors.Add(Instance);
    }
}

void UCandyFightElementAuraComponent::UpdateOrbs()
{
    if (!bShowAuraOrbs)
    {
        for (UStaticMeshComponent* Orb : AuraOrbs) if (Orb) Orb->SetVisibility(false);
        return;
    }
    TArray<ECandyFightElement> Active;
    for (const ECandyFightElement Element : {ECandyFightElement::Fire, ECandyFightElement::Water, ECandyFightElement::Ice})
        if (HasAura(Element)) Active.Add(Element);
    if (Active.IsEmpty())
    {
        for (UStaticMeshComponent* Orb : AuraOrbs) if (Orb) Orb->SetVisibility(false);
        return;
    }
    EnsureOrbs();
    AActor* Owner = GetOwner();
    if (!Owner) return;
    FVector Base = Owner->GetActorLocation() + FVector(0., 0., FMath::Max(0.f, AuraOrbHeight));
    FVector Right = Owner->GetActorRightVector();
    if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
        Right = FRotationMatrix(Camera->GetCameraRotation()).GetUnitAxis(EAxis::Y);
    const float Spacing = FMath::Max(1.f, AuraOrbSpacing);
    const float Scale = FMath::Max(4.f, AuraOrbSize) / 100.f;
    for (int32 Index = 0; Index < AuraOrbs.Num(); ++Index)
    {
        UStaticMeshComponent* Orb = AuraOrbs[Index];
        if (!Orb) continue;
        if (!Active.IsValidIndex(Index))
        {
            Orb->SetVisibility(false);
            continue;
        }
        if (UMaterialInstanceDynamic* Instance = AuraOrbColors.IsValidIndex(Index) ? AuraOrbColors[Index].Get() : nullptr)
            Instance->SetVectorParameterValue(TEXT("Color"), CandyFightElements::GetElementColor(Active[Index]));
        const float Offset = (Index - (Active.Num() - 1) * .5f) * Spacing;
        Orb->SetWorldLocation(Base + Right * Offset);
        Orb->SetWorldScale3D(FVector(Scale));
        Orb->SetVisibility(true);
    }
}

void UCandyFightElementAuraComponent::FaceCamera()
{
    if (!AuraLabel || !AuraLabel->IsVisible()) return;
    if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const FVector ToCamera = Camera->GetCameraLocation() - AuraLabel->GetComponentLocation();
        if (!ToCamera.IsNearlyZero()) AuraLabel->SetWorldRotation(ToCamera.Rotation());
    }
}

void UCandyFightElementAuraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceToNow();
    if (!Auras.IsEmpty()) UpdateLabel();
    FaceCamera();
}
