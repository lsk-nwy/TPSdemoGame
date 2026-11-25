#include "CandyFight/Inventory/CandyFightItemPickup.h"
#include "CandyFight/UI/CandyFightItemPickupWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACandyFightItemPickup::ACandyFightItemPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    SetCanBeDamaged(false);

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
    Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(.55f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);

    PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
    PromptWidget->SetupAttachment(Root);
    PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 78.f));
    PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
    PromptWidget->SetDrawSize(FVector2D(190.f, 62.f));
    PromptWidget->SetPivot(FVector2D(.5f, 0.f));
    PromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PromptWidget->SetGenerateOverlapEvents(false);
    PromptWidget->SetCanEverAffectNavigation(false);
    PromptWidget->SetWidgetClass(UCandyFightItemPickupWidget::StaticClass());
    PromptWidget->SetVisibility(false);
}

void ACandyFightItemPickup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshPresentation();
}

void ACandyFightItemPickup::BeginPlay()
{
    Super::BeginPlay();
    BaseMeshLocation = Mesh ? Mesh->GetRelativeLocation() : FVector::ZeroVector;
    ApplyColor();
    if (PromptWidget && !PromptWidget->GetUserWidgetObject()) PromptWidget->InitWidget();
    RefreshPresentation();
    const UWorld* World = GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.;
    ReadyTime = Now + (FMath::IsFinite(ArmDelay) ? FMath::Max(0.f, ArmDelay) : 0.f);
    if (PromptWidget) PromptWidget->SetVisibility(false);
}

void ACandyFightItemPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Mesh) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
    if (FMath::IsFinite(SpinSpeed) && !FMath::IsNearlyZero(SpinSpeed))
    {
        Mesh->AddRelativeRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f));
    }
    if (bFocused)
    {
        const float Bob = FMath::IsFinite(BobHeight) ? FMath::Max(0.f, BobHeight) : 0.f;
        Mesh->SetRelativeLocation(BaseMeshLocation + FVector(0.f, 0.f, FMath::Sin(Now * 2.4) * Bob));
    }
    else if (!Mesh->GetRelativeLocation().Equals(BaseMeshLocation))
    {
        Mesh->SetRelativeLocation(BaseMeshLocation);
    }
}

bool ACandyFightItemPickup::SetItemId(FName NewItemId)
{
    FCandyFightItemDefinition Item;
    if (!UCandyFightItemLibrary::FindItem(NewItemId, Item))
    {
        UE_LOG(LogTemp, Warning, TEXT("CandyFight item pickup: unknown item id '%s'."), *NewItemId.ToString());
        return false;
    }
    ItemId = NewItemId;
    Definition = Item;
    RefreshPresentation();
    if (HasActorBegunPlay()) ApplyColor();
    return true;
}

FText ACandyFightItemPickup::GetDisplayName() const
{
    return Definition.IsValid() ? Definition.DisplayName : NSLOCTEXT("CandyFightItemUI", "UnknownItem", "未知物品");
}

bool ACandyFightItemPickup::IsPickupReady() const
{
    if (IsActorBeingDestroyed() || !GetWorld()) return false;
    return GetWorld()->GetTimeSeconds() >= ReadyTime;
}

void ACandyFightItemPickup::SetFocused(bool bNewFocused)
{
    if (bFocused == bNewFocused) return;
    bFocused = bNewFocused;
    if (PromptWidget)
    {
        if (bNewFocused)
        {
            if (!PromptWidget->GetUserWidgetObject()) PromptWidget->InitWidget();
            PromptWidget->SetVisibility(true);
        }
        else
        {
            PromptWidget->SetVisibility(false);
        }
    }
    if (bNewFocused) RefreshPresentation();
}

void ACandyFightItemPickup::SetPromptKey(const FText& Key)
{
    if (PromptKey.EqualTo(Key)) return;
    PromptKey = Key;
    RefreshPresentation();
}

void ACandyFightItemPickup::Consume()
{
    SetFocused(false);
    Destroy();
}

void ACandyFightItemPickup::RefreshPresentation()
{
    if (!Definition.IsValid() && !UCandyFightItemLibrary::FindItem(ItemId, Definition))
    {
        Definition = FCandyFightItemDefinition();
    }
    if (Mesh)
    {
        const float Scale = Definition.IsValid() && FMath::IsFinite(Definition.WorldScale)
            ? FMath::Max(.05f, Definition.WorldScale) : 1.f;
        Mesh->SetRelativeScale3D(FVector(.55f * Scale));
    }
    if (PromptWidget)
    {
        if (auto* Widget = Cast<UCandyFightItemPickupWidget>(PromptWidget->GetUserWidgetObject()))
        {
            Widget->SetDisplay(GetDisplayName(), PromptKey);
        }
    }
}

void ACandyFightItemPickup::ApplyColor()
{
    if (!Mesh || !Definition.IsValid()) return;
    if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
    {
        Material->SetVectorParameterValue(TEXT("Color"), Definition.Color);
    }
}

ACandyFightItemPickup* ACandyFightItemPickup::SpawnItem(UWorld* World, FName ItemId, const FVector& Location,
    TSubclassOf<ACandyFightItemPickup> Class, AActor* Owner, float ArmDelay)
{
    if (!World) return nullptr;
    FCandyFightItemDefinition Item;
    if (!UCandyFightItemLibrary::FindItem(ItemId, Item)) return nullptr;
    TSubclassOf<ACandyFightItemPickup> SpawnClass = Class;
    if (!SpawnClass) SpawnClass = ACandyFightItemPickup::StaticClass();
    const FTransform Transform(FRotator::ZeroRotator, Location);
    auto* Pickup = World->SpawnActorDeferred<ACandyFightItemPickup>(SpawnClass, Transform, Owner, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Pickup) return nullptr;
    Pickup->ArmDelay = FMath::IsFinite(ArmDelay) ? FMath::Max(0.f, ArmDelay) : 0.f;
    Pickup->SetItemId(ItemId);
    Pickup->FinishSpawning(Transform);
    return Pickup;
}
