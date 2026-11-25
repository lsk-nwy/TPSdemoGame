#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CandyFightInventoryWidget.generated.h"

class UCandyFightInventoryComponent;
class UGridPanel;
class UTextBlock;
class UWidgetTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightInventorySlotClicked, int32, InSlotIndex);

/** 带格子下标的按钮；父界面只需要一个回调就能处理全部 36 个格子。 */
UCLASS()
class TPSDEMO_API UCandyFightInventorySlotButton : public UButton
{
    GENERATED_BODY()
public:
    UCandyFightInventorySlotButton();

    UPROPERTY(BlueprintReadWrite, Category="Inventory UI")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintAssignable, Category="Inventory UI")
    FCandyFightInventorySlotClicked OnSlotClicked;

    virtual void PostInitProperties() override;

private:
    UFUNCTION()
    void HandleClicked();
};

/** 6×6 背包界面：原生 C++ UMG，不依赖额外蓝图层。 */
UCLASS(Blueprintable)
class TPSDEMO_API UCandyFightInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UCandyFightInventoryWidget(const FObjectInitializer& Initializer);

    void SetInventory(UCandyFightInventoryComponent* InInventory);

    UFUNCTION(BlueprintCallable, Category="Inventory UI")
    void SelectSlot(int32 NewSlot);

    UFUNCTION(BlueprintPure, Category="Inventory UI")
    int32 GetSelectedSlot() const { return SelectedSlot; }

    UFUNCTION(BlueprintCallable, Category="Inventory UI")
    void Refresh();

    static void BuildDefaultLayout(UWidgetTree* Tree);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    friend class FCandyFightInventoryWidgetTest;

    UPROPERTY(Transient)
    TObjectPtr<UCandyFightInventoryComponent> Inventory;

    UPROPERTY(Transient)
    TObjectPtr<UGridPanel> Grid;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCandyFightInventorySlotButton>> SlotButtons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SlotTexts;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CapacityText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SelectedNameText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SelectedDescriptionText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SelectedEffectText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> UseButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> DropButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CloseButton;

    int32 SelectedSlot = 0;
    FText StatusMessage;

    void CacheWidgets();

    UFUNCTION()
    void HandleSlotClicked(int32 InSlotIndex);

    UFUNCTION()
    void HandleUse();

    UFUNCTION()
    void HandleDrop();

    UFUNCTION()
    void HandleClose();

    UFUNCTION()
    void HandleInventoryChanged();
};
