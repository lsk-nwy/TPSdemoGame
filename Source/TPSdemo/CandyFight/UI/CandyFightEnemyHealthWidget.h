#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CandyFightEnemyHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(Blueprintable)
class TPSDEMO_API UCandyFightEnemyHealthWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Enemy Health")
    void SetHealthDisplay(float Current, float Max);
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Enemy Health") TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Enemy Health") TObjectPtr<UTextBlock> HealthText;
    static void BuildDefaultLayout(UWidgetTree* Tree);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
private:
    float CurrentHealth = 100.f;
    float MaximumHealth = 100.f;
    void Refresh();
};
