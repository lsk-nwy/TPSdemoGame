#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFightItemPickupWidget.generated.h"

class UTextBlock;
class UWidgetTree;

/** 世界掉落物上方的简短提示：物品名 + 交互键。 */
UCLASS(Blueprintable)
class TPSDEMO_API UCandyFightItemPickupWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Item UI")
    TObjectPtr<UTextBlock> ItemNameText;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Item UI")
    TObjectPtr<UTextBlock> PickupKeyText;

    UFUNCTION(BlueprintCallable, Category="Item UI")
    void SetDisplay(const FText& ItemName, const FText& Key);

    static void BuildDefaultLayout(UWidgetTree* Tree);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    FText PendingName;
    FText PendingKey = NSLOCTEXT("CandyFightItemUI", "DefaultPickupKey", "E 拾取");
    void ApplyDisplay();
};
