#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFightEscapeGateWidget.generated.h"

class UTextBlock;
class UProgressBar;

UCLASS(Blueprintable)
class TPSDEMO_API UCandyFightEscapeGateWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Escape UI") TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Escape UI") TObjectPtr<UTextBlock> CountdownText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Escape UI") TObjectPtr<UProgressBar> EscapeProgress;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Escape UI") TObjectPtr<UTextBlock> EscapeText;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escape UI") FText ClosingTitle = NSLOCTEXT("CandyFight", "GateClosingTitle", "关闭倒计时");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escape UI") FText ClosedText = NSLOCTEXT("CandyFight", "GateClosed", "已关闭");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escape UI") FText SuccessText = NSLOCTEXT("CandyFight", "GateSuccess", "逃生成功");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escape UI") FText WaitingText = NSLOCTEXT("CandyFight", "GateWaiting", "逃生中 · {Seconds} 秒");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Escape UI", meta=(ClampMin="0",Units="s")) float SuccessSeconds = 2.f;
    UFUNCTION(BlueprintCallable, Category="Escape UI") void SetDisplay(bool bOpen, float CloseSeconds, bool bWaiting, float Progress, float EscapeSeconds, bool bSuccess);
    static void BuildDefaultLayout(UWidgetTree* Tree);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
};
