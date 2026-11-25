#include "CandyFightFlowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

UCandyFightFlowWidget::UCandyFightFlowWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
}

void UCandyFightFlowWidget::Display(ECandyFightFlowScreen NewScreen, const FCandyFightRunSummary& Summary)
{
    CurrentScreen = NewScreen;
    Result = Summary;
    if (WidgetTree) BuildLayout();
}

TSharedRef<SWidget> UCandyFightFlowWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildLayout();
    return Super::RebuildWidget();
}

UButton* UCandyFightFlowWidget::AddButton(UVerticalBox* Box, FName Name, const FString& Label)
{
    auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetBackgroundColor(FLinearColor(.12f, .32f, .38f, 1.f));
    auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    auto Font = Text->GetFont(); Font.Size = 21; Text->SetFont(Font);
    Text->SetMargin(FMargin(18.f, 11.f));
    Text->SetJustification(ETextJustify::Center);
    Button->SetContent(Text);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 5.f));
    return Button;
}

void UCandyFightFlowWidget::BuildLayout()
{
    if (WidgetTree->RootWidget) return;
    auto* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FlowBackground"));
    Background->SetBrushColor(FLinearColor(.025f, .04f, .065f, .98f));
    Background->SetPadding(FMargin(32.f));
    Background->SetHorizontalAlignment(HAlign_Center);
    Background->SetVerticalAlignment(VAlign_Center);
    WidgetTree->RootWidget = Background;
    auto* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(560.f); Background->SetContent(Width);
    auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(); Width->SetContent(Box);

    FString Title, Description, PrimaryLabel, SecondaryLabel;
    switch (CurrentScreen)
    {
    case ECandyFightFlowScreen::MainMenu:
        Title = TEXT("糖块突围"); Description = TEXT("搜集物资，迎战敌人，带着收获撤离。");
        PrimaryLabel = TEXT("开始游戏"); SecondaryLabel = TEXT("退出游戏"); break;
    case ECandyFightFlowScreen::Preparation:
        Title = TEXT("备战"); Description = TEXT("准备好后即可进入战斗。\n战斗中按 B 打开 6×6 背包，按 Esc 关闭。");
        PrimaryLabel = TEXT("进入战斗"); SecondaryLabel = TEXT("返回主菜单"); break;
    case ECandyFightFlowScreen::Backpack:
        Title = TEXT("背包"); Description = TEXT("战斗中按 B 打开 6×6 背包。\n每个格子放 1 件物品，可拾取、使用和丢弃。\n备战界面暂只提供这个入口。"); PrimaryLabel = TEXT("返回备战"); break;
    case ECandyFightFlowScreen::Warehouse:
        Title = TEXT("仓库"); Description = TEXT("仓库入口已接通。\n存放、取出和持久化将在后续实现。"); PrimaryLabel = TEXT("返回备战"); break;
    case ECandyFightFlowScreen::Shop:
        Title = TEXT("商店"); Description = TEXT("商店入口已接通。\n商品、购买和出售将在后续实现。"); PrimaryLabel = TEXT("返回备战"); break;
    case ECandyFightFlowScreen::Equipment:
        Title = TEXT("装备"); Description = TEXT("装备入口已接通。\n本次沿用战斗关卡原有的默认武器配置。"); PrimaryLabel = TEXT("返回备战"); break;
    case ECandyFightFlowScreen::Loading:
        Title = TEXT("正在进入关卡……"); Description = TEXT("请稍候。"); break;
    case ECandyFightFlowScreen::Pause:
        Title = TEXT("战斗已暂停"); Description = TEXT("可以继续战斗，或放弃本局并查看结算。");
        PrimaryLabel = TEXT("继续战斗"); SecondaryLabel = TEXT("放弃本局并结算"); break;
    case ECandyFightFlowScreen::Settlement:
        Title = UCandyFightGameInstance::OutcomeText(Result.Outcome);
        Description = FString::Printf(TEXT("本局搜得金币：%d\n战斗时长：%02d:%02d\n\n本次仅展示结果，不入库、不扣款、不存档。"),
            Result.Gold, int32(Result.ElapsedSeconds) / 60, int32(Result.ElapsedSeconds) % 60);
        PrimaryLabel = TEXT("返回备战"); SecondaryLabel = TEXT("返回主菜单"); break;
    default: break;
    }
    auto* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FlowTitle"));
    Heading->SetText(FText::FromString(Title)); auto Font = Heading->GetFont(); Font.Size = 34; Heading->SetFont(Font);
    Heading->SetJustification(ETextJustify::Center); Box->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    auto* Body = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FlowDescription"));
    Body->SetText(FText::FromString(Description)); Body->SetAutoWrapText(true); Body->SetJustification(ETextJustify::Center);
    Font = Body->GetFont(); Font.Size = 18; Body->SetFont(Font); Body->SetColorAndOpacity(FSlateColor(FLinearColor(.76f, .82f, .88f)));
    Box->AddChildToVerticalBox(Body)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
    if (CurrentScreen == ECandyFightFlowScreen::Preparation)
    {
        AddButton(Box, TEXT("FlowBackpack"), TEXT("背包"))->OnClicked.AddDynamic(this, &ThisClass::Backpack);
        AddButton(Box, TEXT("FlowWarehouse"), TEXT("仓库"))->OnClicked.AddDynamic(this, &ThisClass::Warehouse);
        AddButton(Box, TEXT("FlowShop"), TEXT("商店"))->OnClicked.AddDynamic(this, &ThisClass::Shop);
        AddButton(Box, TEXT("FlowEquipment"), TEXT("装备"))->OnClicked.AddDynamic(this, &ThisClass::Equipment);
    }
    if (!PrimaryLabel.IsEmpty()) AddButton(Box, TEXT("FlowPrimary"), PrimaryLabel)->OnClicked.AddDynamic(this, &ThisClass::Primary);
    if (!SecondaryLabel.IsEmpty()) AddButton(Box, TEXT("FlowSecondary"), SecondaryLabel)->OnClicked.AddDynamic(this, &ThisClass::Secondary);
}

UCandyFightGameInstance* UCandyFightFlowWidget::Flow() const { return Cast<UCandyFightGameInstance>(GetGameInstance()); }

void UCandyFightFlowWidget::Primary()
{
    auto* Game = Flow(); if (!Game) return;
    switch (CurrentScreen)
    {
    case ECandyFightFlowScreen::Preparation: Game->StartBattle(); break;
    case ECandyFightFlowScreen::Pause: Game->ToggleBattleMenu(); break;
    case ECandyFightFlowScreen::Settlement: Game->ReturnFromBattle(); break;
    default: Game->ShowPreparation(); break;
    }
}

void UCandyFightFlowWidget::Secondary()
{
    auto* Game = Flow(); if (!Game) return;
    switch (CurrentScreen)
    {
    case ECandyFightFlowScreen::MainMenu: UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); break;
    case ECandyFightFlowScreen::Preparation: Game->ShowMainMenu(); break;
    case ECandyFightFlowScreen::Pause: Game->FinishBattle(ECandyFightRunOutcome::Abandoned); break;
    case ECandyFightFlowScreen::Settlement: Game->ReturnFromBattle(true); break;
    default: break;
    }
}

void UCandyFightFlowWidget::Backpack() { if (auto* Game = Flow()) Game->OpenPreparationPanel(ECandyFightFlowScreen::Backpack); }
void UCandyFightFlowWidget::Warehouse() { if (auto* Game = Flow()) Game->OpenPreparationPanel(ECandyFightFlowScreen::Warehouse); }
void UCandyFightFlowWidget::Shop() { if (auto* Game = Flow()) Game->OpenPreparationPanel(ECandyFightFlowScreen::Shop); }
void UCandyFightFlowWidget::Equipment() { if (auto* Game = Flow()) Game->OpenPreparationPanel(ECandyFightFlowScreen::Equipment); }

FReply UCandyFightFlowWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || (Event.GetKey() == EKeys::Tab && CurrentScreen == ECandyFightFlowScreen::Pause))
    {
        if (auto* Game = Flow())
        {
            if (CurrentScreen == ECandyFightFlowScreen::Pause) Game->ToggleBattleMenu();
            else if (CurrentScreen == ECandyFightFlowScreen::Backpack || CurrentScreen == ECandyFightFlowScreen::Warehouse ||
                CurrentScreen == ECandyFightFlowScreen::Shop || CurrentScreen == ECandyFightFlowScreen::Equipment) Game->ShowPreparation();
            else return Super::NativeOnPreviewKeyDown(Geometry, Event);
        }
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}