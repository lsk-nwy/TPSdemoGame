#include "CandyFight/UI/CandyFightItemPickupWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UCandyFightItemPickupWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
    if (!Tree || Tree->RootWidget) return;
    auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    Tree->RootWidget = Canvas;

    auto* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PickupPanel"));
    Panel->SetBrushColor(FLinearColor(.02f, .03f, .05f, .82f));
    Panel->SetPadding(FMargin(10.f, 6.f));
    Panel->SetHorizontalAlignment(HAlign_Center);
    Panel->SetVerticalAlignment(VAlign_Center);
    auto* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(.5f, 0.f));
    PanelSlot->SetAlignment(FVector2D(.5f, 0.f));
    PanelSlot->SetPosition(FVector2D::ZeroVector);
    PanelSlot->SetSize(FVector2D(190.f, 62.f));

    auto* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PickupContents"));
    Panel->SetContent(Box);

    auto* Name = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemNameText"));
    FSlateFontInfo Font = Name->GetFont();
    Font.Size = 16;
    Name->SetFont(Font);
    Name->SetColorAndOpacity(FSlateColor(FLinearColor(.96f, .98f, 1.f)));
    Name->SetJustification(ETextJustify::Center);
    Name->SetText(FText::FromString(TEXT("物品")));
    Box->AddChildToVerticalBox(Name);

    auto* Key = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PickupKeyText"));
    Font = Key->GetFont();
    Font.Size = 12;
    Key->SetFont(Font);
    Key->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .8f, .3f)));
    Key->SetJustification(ETextJustify::Center);
    Key->SetText(FText::FromString(TEXT("E 拾取")));
    Box->AddChildToVerticalBox(Key)->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
}

TSharedRef<SWidget> UCandyFightItemPickupWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultLayout(WidgetTree);
        ItemNameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ItemNameText")));
        PickupKeyText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PickupKeyText")));
    }
    return Super::RebuildWidget();
}

void UCandyFightItemPickupWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ApplyDisplay();
}

void UCandyFightItemPickupWidget::SetDisplay(const FText& ItemName, const FText& Key)
{
    PendingName = ItemName;
    PendingKey = Key;
    ApplyDisplay();
}

void UCandyFightItemPickupWidget::ApplyDisplay()
{
    if (ItemNameText) ItemNameText->SetText(PendingName);
    if (PickupKeyText) PickupKeyText->SetText(PendingKey);
}
