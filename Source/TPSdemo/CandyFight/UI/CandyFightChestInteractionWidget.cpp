#include "CandyFightChestInteractionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

void UCandyFightChestInteractionWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
    if (!Tree || Tree->RootWidget) return;
    auto* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    Tree->RootWidget = Root; Root->SetVisibility(ESlateVisibility::HitTestInvisible);
    const auto Place = [&](UWidget* Child, FVector2D Position, FVector2D Size)
    {
        auto* Slot = Root->AddChildToCanvas(Child); Slot->SetAnchors(FAnchors(.5f,0.f));
        Slot->SetAlignment(FVector2D(.5,0)); Slot->SetPosition(Position); Slot->SetSize(Size);
    };
    auto* Ring = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SearchRing"));
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CandyFight/Materials/UI/MI_ChestSearchRing"))) Ring->SetBrushFromMaterial(Material);
    Place(Ring, FVector2D(0,0), FVector2D(64,64));
    auto* Key = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyText"));
    auto Font = Key->GetFont(); Font.Size = 20; Key->SetFont(Font); Key->SetJustification(ETextJustify::Center);
    Key->SetText(FText::FromString(TEXT("E"))); Place(Key, FVector2D(0,18), FVector2D(44,28));
    auto* Prompt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
    Font.Size = 14; Prompt->SetFont(Font); Prompt->SetJustification(ETextJustify::Center);
    Prompt->SetAutoWrapText(true); Prompt->SetText(FText::FromString(TEXT("长按 E 搜索")));
    Place(Prompt, FVector2D(0,70), FVector2D(160,42));
}
TSharedRef<SWidget> UCandyFightChestInteractionWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultLayout(WidgetTree);
        SearchRing = Cast<UImage>(WidgetTree->FindWidget(TEXT("SearchRing")));
        KeyText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("KeyText")));
        PromptText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PromptText")));
    }
    return Super::RebuildWidget();
}
void UCandyFightChestInteractionWidget::NativeConstruct()
{
    Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible); UpdateRing();
}
void UCandyFightChestInteractionWidget::SetDisplay(ECandyFightChestPrompt Mode, float Progress, FText Key, int32 Gold)
{
    if (Mode == ECandyFightChestPrompt::Searching && DisplayMode != Mode) RotationStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
    DisplayMode = Mode;
    DisplayProgress = FMath::IsFinite(Progress) ? FMath::Clamp(Progress,0.f,1.f) : 0.f;
    if (KeyText) KeyText->SetText(Key);
    if (PromptText)
    {
        FFormatNamedArguments Args; Args.Add(TEXT("Key"),Key); Args.Add(TEXT("Gold"),Gold);
        PromptText->SetText(FText::Format(Mode == ECandyFightChestPrompt::Looted ? RewardText :
            Mode == ECandyFightChestPrompt::Interrupted ? InterruptedText : Mode == ECandyFightChestPrompt::Searching ? SearchingText : ReadyText, Args));
    }
    UpdateRing();
}
void UCandyFightChestInteractionWidget::UpdateRing()
{
    if (!RingMaterial && SearchRing) RingMaterial = SearchRing->GetDynamicMaterial();
    if (!RingMaterial) return;
    const bool bRotate = DisplayMode == ECandyFightChestPrompt::Searching;
    const double Elapsed = GetWorld() ? FMath::Max(0., GetWorld()->GetTimeSeconds()-RotationStart) : 0.;
    const float Speed = FMath::IsFinite(RotationsPerSecond) ? FMath::Max(0.f, RotationsPerSecond) : 0.f;
    RingMaterial->SetScalarParameterValue(TEXT("Progress"),DisplayProgress);
    RingMaterial->SetScalarParameterValue(TEXT("Phase"),bRotate ? FMath::Frac(Elapsed*Speed) : 0.f);
    RingMaterial->SetScalarParameterValue(TEXT("ShowDot"),bRotate ? 1.f : 0.f);
}
void UCandyFightChestInteractionWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime); UpdateRing();
}
