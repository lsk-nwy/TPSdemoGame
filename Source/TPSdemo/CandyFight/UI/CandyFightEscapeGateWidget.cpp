#include "CandyFightEscapeGateWidget.h"
#include "CandyFightWeaponHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UCandyFightEscapeGateWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
    if (!Tree || Tree->RootWidget) return;
    auto* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas"));
    Tree->RootWidget=Root; Root->SetVisibility(ESlateVisibility::HitTestInvisible);
    const auto Place=[Root](UWidget* Child,FVector2D Position,FVector2D Size)
    {
        auto* Slot=Root->AddChildToCanvas(Child); Slot->SetPosition(Position); Slot->SetSize(Size);
    };
    auto* Background=Tree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("PanelBackground"));
    Background->SetColorAndOpacity(FLinearColor(.025f,.035f,.045f,.8f)); Place(Background,{0,0},{180,96});
    const auto Text=[Tree,&Place](const TCHAR* Name,const TCHAR* Preview,int32 Size,FVector2D Position,FVector2D Dimensions,FLinearColor Color)
    {
        auto* Block=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=Block->GetFont(); Font.Size=Size; Block->SetFont(Font); Block->SetJustification(ETextJustify::Center);
        Block->SetColorAndOpacity(Color); Block->SetText(FText::FromString(Preview)); Place(Block,Position,Dimensions); return Block;
    };
    Text(TEXT("TitleText"),TEXT("关闭倒计时"),14,{8,4},{164,22},FLinearColor(.75f,.8f,.82f));
    Text(TEXT("CountdownText"),TEXT("02:00"),26,{8,24},{164,34},FLinearColor::White);
    auto* Progress=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("EscapeProgress"));
    Progress->SetFillColorAndOpacity(FLinearColor(.05f,.85f,.7f)); Progress->SetPercent(.4f); Place(Progress,{12,61},{156,6});
    Text(TEXT("EscapeText"),TEXT("逃生中 · 3 秒"),13,{8,70},{164,22},FLinearColor::White);
}
TSharedRef<SWidget> UCandyFightEscapeGateWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultLayout(WidgetTree);
        TitleText=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TitleText")));
        CountdownText=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CountdownText")));
        EscapeProgress=Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("EscapeProgress")));
        EscapeText=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("EscapeText")));
    }
    return Super::RebuildWidget();
}
void UCandyFightEscapeGateWidget::NativeConstruct()
{
    Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UCandyFightEscapeGateWidget::SetDisplay(bool bOpen,float CloseSeconds,bool bWaiting,float Progress,float EscapeSeconds,bool bSuccess)
{
    if (TitleText) TitleText->SetText(!bOpen ? ClosedText : bSuccess ? SuccessText : ClosingTitle);
    if (CountdownText)
    {
        CountdownText->SetText(UCandyFightWeaponHUD::FormatCountdown(CloseSeconds));
        CountdownText->SetVisibility(bOpen && !bSuccess ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    const bool ShowWait=bOpen && bWaiting && !bSuccess;
    if (EscapeProgress)
    {
        EscapeProgress->SetPercent(FMath::IsFinite(Progress) ? FMath::Clamp(Progress,0.f,1.f) : 0.f);
        EscapeProgress->SetVisibility(ShowWait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (EscapeText)
    {
        FFormatNamedArguments Args;
        Args.Add(TEXT("Seconds"),FMath::IsFinite(EscapeSeconds) ? FMath::CeilToInt(FMath::Clamp(EscapeSeconds,0.f,2147483520.f)) : 0);
        EscapeText->SetText(FText::Format(WaitingText,Args));
        EscapeText->SetVisibility(ShowWait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}
