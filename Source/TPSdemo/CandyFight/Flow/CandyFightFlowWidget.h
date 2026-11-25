#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFightFlow.h"
#include "CandyFightFlowWidget.generated.h"

class UVerticalBox;
class UButton;

// 最小可运行 UI：全部由 C++ 创建，后续可替换成 UMG 美术界面。
UCLASS()
class TPSDEMO_API UCandyFightFlowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UCandyFightFlowWidget(const FObjectInitializer& Initializer);
    void Display(ECandyFightFlowScreen NewScreen, const FCandyFightRunSummary& Summary);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    ECandyFightFlowScreen CurrentScreen = ECandyFightFlowScreen::MainMenu;
    FCandyFightRunSummary Result;
    void BuildLayout();
    UButton* AddButton(UVerticalBox* Box, FName Name, const FString& Label);
    UCandyFightGameInstance* Flow() const;
    UFUNCTION() void Primary();
    UFUNCTION() void Secondary();
    UFUNCTION() void Backpack();
    UFUNCTION() void Warehouse();
    UFUNCTION() void Shop();
    UFUNCTION() void Equipment();
};