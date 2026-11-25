#include "CandyFight/UI/CandyFightInventoryWidget.h"
#include "CandyFight/Inventory/CandyFightInventoryComponent.h"
#include "CandyFight/Inventory/CandyFightItem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

namespace
{
    UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FString& Text, int32 Size,
        const FLinearColor& Color, ETextJustify::Type Justify = ETextJustify::Left)
    {
        auto* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        FSlateFontInfo Font = Block->GetFont();
        Font.Size = Size;
        Block->SetFont(Font);
        Block->SetColorAndOpacity(FSlateColor(Color));
        Block->SetJustification(Justify);
        Block->SetAutoWrapText(true);
        Block->SetText(FText::FromString(Text));
        return Block;
    }

    UButton* MakeActionButton(UWidgetTree* Tree, UHorizontalBox* Box, FName Name, const FString& Label)
    {
        auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        Button->SetBackgroundColor(FLinearColor(.12f, .32f, .4f, 1.f));
        auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Label"))));
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = 18;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(FLinearColor(.96f, .98f, 1.f)));
        Text->SetJustification(ETextJustify::Center);
        Text->SetText(FText::FromString(Label));
        Button->SetContent(Text);
        auto* Slot = Box->AddChildToHorizontalBox(Button);
        Slot->SetPadding(FMargin(7.f, 0.f));
        Slot->SetVerticalAlignment(VAlign_Center);
        return Button;
    }
}

UCandyFightInventorySlotButton::UCandyFightInventorySlotButton()
{
    SlotIndex = INDEX_NONE;
}

void UCandyFightInventorySlotButton::PostInitProperties()
{
    Super::PostInitProperties();
    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
    }
}

void UCandyFightInventorySlotButton::HandleClicked()
{
    OnSlotClicked.Broadcast(SlotIndex);
}

UCandyFightInventoryWidget::UCandyFightInventoryWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
}

void UCandyFightInventoryWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
    if (!Tree || Tree->RootWidget) return;
    auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
    Canvas->SetVisibility(ESlateVisibility::Visible);
    Tree->RootWidget = Canvas;

    auto* Backdrop = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryBackdrop"));
    Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, .72f));
    auto* BackdropSlot = Canvas->AddChildToCanvas(Backdrop);
    BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    BackdropSlot->SetOffsets(FMargin(0.f));

    auto* Window = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryWindow"));
    Window->SetBrushColor(FLinearColor(.025f, .042f, .065f, .985f));
    Window->SetPadding(FMargin(26.f, 20.f));
    auto* WindowSlot = Canvas->AddChildToCanvas(Window);
    WindowSlot->SetAnchors(FAnchors(.5f, .5f));
    WindowSlot->SetAlignment(FVector2D(.5f, .5f));
    WindowSlot->SetPosition(FVector2D::ZeroVector);
    WindowSlot->SetSize(FVector2D(700.f, 828.f));

    auto* Content = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryContent"));
    Window->SetContent(Content);

    auto* Title = MakeText(Tree, TEXT("InventoryTitle"), TEXT("背包 · 6 × 6"), 30,
        FLinearColor(.97f, .98f, 1.f), ETextJustify::Center);
    Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));

    auto* Hint = MakeText(Tree, TEXT("InventoryHint"), TEXT("B / ESC 关闭 · 点击格子选择物品"), 14,
        FLinearColor(.64f, .71f, .79f), ETextJustify::Center);
    Content->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

    auto* Capacity = MakeText(Tree, TEXT("InventoryCapacity"), TEXT("已使用 0 / 36"), 16,
        FLinearColor(1.f, .8f, .3f), ETextJustify::Center);
    Content->AddChildToVerticalBox(Capacity)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    auto* Grid = Tree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("InventoryGrid"));
    auto* GridSlot = Content->AddChildToVerticalBox(Grid);
    GridSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    GridSlot->SetHorizontalAlignment(HAlign_Center);
    for (int32 Row = 0; Row < 6; ++Row)
    {
        for (int32 Column = 0; Column < 6; ++Column)
        {
            const int32 Index = Row * 6 + Column;
            auto* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
                FName(*FString::Printf(TEXT("InventorySlotSize%d"), Index)));
            Size->SetWidthOverride(66.f);
            Size->SetHeightOverride(66.f);

            auto* Button = Tree->ConstructWidget<UCandyFightInventorySlotButton>(
                UCandyFightInventorySlotButton::StaticClass(),
                FName(*FString::Printf(TEXT("InventorySlot%d"), Index)));
            Button->SlotIndex = Index;
            Button->SetBackgroundColor(FLinearColor(.07f, .09f, .12f, 1.f));
            auto* Label = MakeText(Tree, FName(*FString::Printf(TEXT("InventorySlotText%d"), Index)),
                TEXT("空"), 13, FLinearColor(.62f, .68f, .75f), ETextJustify::Center);
            Label->SetAutoWrapText(true);
            Button->SetContent(Label);
            Size->SetContent(Button);

            auto* Cell = Grid->AddChildToGrid(Size, Row, Column);
            Cell->SetPadding(FMargin(4.f));
            Cell->SetHorizontalAlignment(HAlign_Fill);
            Cell->SetVerticalAlignment(VAlign_Fill);
        }
    }

    auto* SelectedName = MakeText(Tree, TEXT("InventorySelectedName"), TEXT("未选择物品"), 22,
        FLinearColor(1.f, .84f, .38f));
    Content->AddChildToVerticalBox(SelectedName)->SetPadding(FMargin(2.f, 0.f, 2.f, 2.f));

    auto* SelectedDescription = MakeText(Tree, TEXT("InventorySelectedDescription"),
        TEXT("点击上方格子选择物品。"), 14, FLinearColor(.74f, .8f, .87f));
    Content->AddChildToVerticalBox(SelectedDescription)->SetPadding(FMargin(2.f, 0.f, 2.f, 2.f));

    auto* SelectedEffect = MakeText(Tree, TEXT("InventorySelectedEffect"), FString(), 15,
        FLinearColor(.44f, .93f, .76f));
    Content->AddChildToVerticalBox(SelectedEffect)->SetPadding(FMargin(2.f, 0.f, 2.f, 2.f));

    auto* Status = MakeText(Tree, TEXT("InventoryStatus"), TEXT("选择一个格子查看物品效果。"), 14,
        FLinearColor(.93f, .76f, .45f));
    Content->AddChildToVerticalBox(Status)->SetPadding(FMargin(2.f, 0.f, 2.f, 4.f));

    auto* Actions = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryActions"));
    auto* ActionSlot = Content->AddChildToVerticalBox(Actions);
    ActionSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
    ActionSlot->SetHorizontalAlignment(HAlign_Center);
    MakeActionButton(Tree, Actions, TEXT("InventoryUseButton"), TEXT("使用"));
    MakeActionButton(Tree, Actions, TEXT("InventoryDropButton"), TEXT("丢弃"));
    MakeActionButton(Tree, Actions, TEXT("InventoryCloseButton"), TEXT("关闭 (ESC)"));

    auto* Footer = MakeText(Tree, TEXT("InventoryFooter"),
        TEXT("每个格子只能放 1 件物品，不堆叠。靠近地面物品按 E 拾取。"), 13,
        FLinearColor(.58f, .64f, .7f), ETextJustify::Center);
    Content->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
}

TSharedRef<SWidget> UCandyFightInventoryWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultLayout(WidgetTree);
    }
    CacheWidgets();
    return Super::RebuildWidget();
}

void UCandyFightInventoryWidget::CacheWidgets()
{
    if (!WidgetTree) return;
    Grid = Cast<UGridPanel>(WidgetTree->FindWidget(TEXT("InventoryGrid")));
    SlotButtons.Reset();
    SlotTexts.Reset();
    for (int32 Index = 0; Index < 36; ++Index)
    {
        auto* Button = Cast<UCandyFightInventorySlotButton>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("InventorySlot%d"), Index))));
        if (Button)
        {
            Button->OnSlotClicked.AddUniqueDynamic(this, &ThisClass::HandleSlotClicked);
        }
        SlotButtons.Add(Button);
        SlotTexts.Add(Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("InventorySlotText%d"), Index)))));
    }
    CapacityText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InventoryCapacity")));
    SelectedNameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InventorySelectedName")));
    SelectedDescriptionText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InventorySelectedDescription")));
    SelectedEffectText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InventorySelectedEffect")));
    StatusText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InventoryStatus")));
    UseButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("InventoryUseButton")));
    DropButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("InventoryDropButton")));
    CloseButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("InventoryCloseButton")));
    if (UseButton) UseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUse);
    if (DropButton) DropButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDrop);
    if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClose);
}

void UCandyFightInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Visible);
    if (StatusMessage.IsEmpty())
    {
        StatusMessage = NSLOCTEXT("CandyFightInventoryUI", "SelectHint", "选择一个格子查看物品效果。");
    }
    Refresh();
    SetKeyboardFocus();
}

void UCandyFightInventoryWidget::NativeDestruct()
{
    if (Inventory)
    {
        Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
    }
    Super::NativeDestruct();
}

void UCandyFightInventoryWidget::SetInventory(UCandyFightInventoryComponent* InInventory)
{
    if (Inventory == InInventory)
    {
        Refresh();
        return;
    }
    if (Inventory) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
    Inventory = InInventory;
    if (Inventory) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
    if (Inventory && !Inventory->Slots.IsValidIndex(SelectedSlot)) SelectedSlot = 0;
    Refresh();
}

void UCandyFightInventoryWidget::SelectSlot(int32 NewSlot)
{
    if (!Inventory || !Inventory->Slots.IsValidIndex(NewSlot)) return;
    SelectedSlot = NewSlot;
    Refresh();
}

void UCandyFightInventoryWidget::Refresh()
{
    if (!Inventory)
    {
        if (CapacityText) CapacityText->SetText(FText::FromString(TEXT("已使用 0 / 36")));
        if (SelectedNameText) SelectedNameText->SetText(FText::FromString(TEXT("未选择物品")));
        if (SelectedDescriptionText) SelectedDescriptionText->SetText(FText::GetEmpty());
        if (SelectedEffectText) SelectedEffectText->SetText(FText::GetEmpty());
        if (StatusText) StatusText->SetText(StatusMessage);
        if (UseButton) UseButton->SetIsEnabled(false);
        if (DropButton) DropButton->SetIsEnabled(false);
        return;
    }

    const int32 SlotCount = Inventory->GetSlotCount();
    for (int32 Index = 0; Index < SlotButtons.Num(); ++Index)
    {
        FCandyFightItemDefinition Item;
        const bool bHasItem = Inventory->GetItemAt(Index, Item);
        const bool bSelected = Index == SelectedSlot;
        if (auto* Label = SlotTexts[Index].Get())
        {
            Label->SetText(bHasItem ? Item.DisplayName : FText::FromString(TEXT("空")));
            Label->SetColorAndOpacity(FSlateColor(bSelected
                ? FLinearColor(1.f, .97f, .85f)
                : bHasItem ? FLinearColor(.94f, .96f, .99f) : FLinearColor(.52f, .58f, .64f)));
        }
        if (auto* Button = SlotButtons[Index].Get())
        {
            FLinearColor Color = FLinearColor(.07f, .09f, .12f, 1.f);
            if (bHasItem)
            {
                Color = FLinearColor(Item.Color.R * .52f, Item.Color.G * .52f, Item.Color.B * .52f, 1.f);
            }
            if (bSelected)
            {
                Color = bHasItem ? FLinearColor(.68f, .52f, .16f, 1.f) : FLinearColor(.23f, .29f, .36f, 1.f);
            }
            Button->SetBackgroundColor(Color);
        }
    }

    FCandyFightItemDefinition Selected;
    const bool bHasSelected = Inventory->GetItemAt(SelectedSlot, Selected);
    if (CapacityText)
    {
        CapacityText->SetText(FText::FromString(FString::Printf(TEXT("已使用 %d / %d"),
            Inventory->GetUsedSlotCount(), SlotCount)));
    }
    if (bHasSelected)
    {
        if (SelectedNameText) SelectedNameText->SetText(Selected.DisplayName);
        if (SelectedDescriptionText) SelectedDescriptionText->SetText(Selected.Description);
        if (SelectedEffectText) SelectedEffectText->SetText(UCandyFightItemLibrary::DescribeEffect(Selected));
    }
    else
    {
        if (SelectedNameText)
        {
            SelectedNameText->SetText(SelectedSlot >= 0 && SelectedSlot < SlotCount
                ? NSLOCTEXT("CandyFightInventoryUI", "EmptySlot", "空格子")
                : NSLOCTEXT("CandyFightInventoryUI", "NoSelection", "未选择物品"));
        }
        if (SelectedDescriptionText) SelectedDescriptionText->SetText(NSLOCTEXT("CandyFightInventoryUI", "EmptyHint", "点击其他格子查看物品。"));
        if (SelectedEffectText) SelectedEffectText->SetText(FText::GetEmpty());
    }
    if (StatusText) StatusText->SetText(StatusMessage);
    if (UseButton) UseButton->SetIsEnabled(bHasSelected && UCandyFightItemLibrary::IsUsable(Selected));
    if (DropButton) DropButton->SetIsEnabled(bHasSelected);
}

void UCandyFightInventoryWidget::HandleSlotClicked(int32 InSlotIndex)
{
    SelectSlot(InSlotIndex);
}

void UCandyFightInventoryWidget::HandleUse()
{
    if (!Inventory) return;
    FText Message;
    Inventory->UseAt(SelectedSlot, Message);
    StatusMessage = Message;
    Refresh();
}

void UCandyFightInventoryWidget::HandleDrop()
{
    if (!Inventory) return;
    FText Message;
    Inventory->DropAt(SelectedSlot, Message);
    StatusMessage = Message;
    Refresh();
}

void UCandyFightInventoryWidget::HandleClose()
{
    if (Inventory) Inventory->CloseInventory();
}

void UCandyFightInventoryWidget::HandleInventoryChanged()
{
    Refresh();
}

FReply UCandyFightInventoryWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::B)
    {
        if (Inventory) Inventory->CloseInventory();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
