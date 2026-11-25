#include "CandyFightEnemyHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UCandyFightEnemyHealthWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
    if (!Tree || Tree->RootWidget) return;
    auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    Tree->RootWidget = Canvas;
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    const auto Place = [&](UWidget* Widget, FMargin Offsets, FAnchors Anchors)
    {
        auto* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetAnchors(Anchors);
        Slot->SetOffsets(Offsets);
    };
    auto* Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
    Background->SetBrushColor(FLinearColor(.025f, .035f, .05f, .85f));
    Place(Background, FMargin(0), FAnchors(0,0,1,1));
    auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
    auto Font = Text->GetFont(); Font.Size = 14; Text->SetFont(Font);
    Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Text->SetJustification(ETextJustify::Center);
    Text->SetText(FText::FromString(TEXT("100 / 100")));
    Place(Text, FMargin(4, 2, 4, 23), FAnchors(0,0,1,0));
    auto* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
    FProgressBarStyle Style;
    Style.BackgroundImage.DrawAs = ESlateBrushDrawType::Box;
    Style.BackgroundImage.TintColor = FLinearColor(.08f,.10f,.13f);
    Style.FillImage.DrawAs = ESlateBrushDrawType::Box;
    Style.FillImage.TintColor = FLinearColor::White;
    Bar->SetWidgetStyle(Style);
    Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
    Bar->SetFillColorAndOpacity(FLinearColor(.85f,.08f,.12f));
    Bar->SetPercent(1.f);
    Place(Bar, FMargin(6, 27, 6, 7), FAnchors(0,0,1,0));
}

TSharedRef<SWidget> UCandyFightEnemyHealthWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultLayout(WidgetTree);
        HealthBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("HealthBar")));
        HealthText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("HealthText")));
    }
    return Super::RebuildWidget();
}

void UCandyFightEnemyHealthWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    Refresh();
}

void UCandyFightEnemyHealthWidget::SetHealthDisplay(float Current, float Max)
{
    MaximumHealth = FMath::IsFinite(Max) ? FMath::Max(0.f, Max) : 0.f;
    CurrentHealth = FMath::IsFinite(Current) ? FMath::Clamp(Current, 0.f, MaximumHealth) : 0.f;
    Refresh();
}

void UCandyFightEnemyHealthWidget::Refresh()
{
    if (HealthBar) HealthBar->SetPercent(MaximumHealth > 0.f ? CurrentHealth / MaximumHealth : 0.f);
    if (HealthText) HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, MaximumHealth)));
}
