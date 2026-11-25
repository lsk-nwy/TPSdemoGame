#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFightChestInteractionWidget.generated.h"

class UImage;
class UTextBlock;
class UMaterialInstanceDynamic;
UENUM(BlueprintType)
enum class ECandyFightChestPrompt : uint8 { Ready, Searching, Interrupted, Looted };

UCLASS(Blueprintable)
class TPSDEMO_API UCandyFightChestInteractionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Chest UI") TObjectPtr<UImage> SearchRing;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Chest UI") TObjectPtr<UTextBlock> KeyText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Chest UI") TObjectPtr<UTextBlock> PromptText;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI") FText ReadyText = NSLOCTEXT("CandyFight", "ChestReady", "长按 {Key} 搜索");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI") FText SearchingText = NSLOCTEXT("CandyFight", "ChestSearching", "搜索中 · 松开取消");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI") FText InterruptedText = NSLOCTEXT("CandyFight", "ChestInterrupted", "搜索已中断");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI") FText RewardText = NSLOCTEXT("CandyFight", "ChestReward", "+{Gold} 金币");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI", meta=(ClampMin="0")) float RotationsPerSecond = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI", meta=(ClampMin="0",Units="s")) float InterruptedSeconds = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chest UI", meta=(ClampMin="0",Units="s")) float RewardSeconds = 2.f;
    UFUNCTION(BlueprintCallable, Category="Chest UI") void SetDisplay(ECandyFightChestPrompt Mode, float Progress, FText Key, int32 Gold);
    static void BuildDefaultLayout(UWidgetTree* Tree);
    UFUNCTION(BlueprintPure, Category="Chest UI") float GetDisplayedProgress() const { return DisplayProgress; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
    ECandyFightChestPrompt DisplayMode = ECandyFightChestPrompt::Ready;
    float DisplayProgress = 0.f;
    double RotationStart = 0.;
    void UpdateRing();
};
