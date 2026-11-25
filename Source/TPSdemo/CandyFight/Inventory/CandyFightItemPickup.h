#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CandyFight/Inventory/CandyFightItem.h"
#include "CandyFightItemPickup.generated.h"

class UCandyFightItemPickupWidget;
class UStaticMeshComponent;
class UWidgetComponent;

/** 可拾取的地面物品。靠近后由背包装配组件显示 E 提示。 */
UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightItemPickup : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightItemPickup();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item Pickup")
    FName ItemId = TEXT("MintCandy");

    /** 掉落或生成后多久才允许拾取，避免丢弃后立刻被自己捡回。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item Pickup", meta=(ClampMin="0", Units="s"))
    float ArmDelay = .5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item Pickup", meta=(ClampMin="0", Units="cm"))
    float BobHeight = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item Pickup", meta=(Units="deg/s"))
    float SpinSpeed = 55.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item Pickup")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item Pickup")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item Pickup")
    TObjectPtr<UWidgetComponent> PromptWidget;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Item Pickup")
    bool bFocused = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Item Pickup")
    FCandyFightItemDefinition Definition;

    UFUNCTION(BlueprintPure, Category="Item Pickup")
    FText GetDisplayName() const;

    UFUNCTION(BlueprintCallable, Category="Item Pickup")
    bool SetItemId(FName NewItemId);

    UFUNCTION(BlueprintCallable, Category="Item Pickup")
    bool IsPickupReady() const;

    /** 由背包组件在最近可拾取物变化时调用。 */
    void SetFocused(bool bNewFocused);
    void SetPromptKey(const FText& Key);
    void Consume();

    virtual void Tick(float DeltaSeconds) override;

    /** 生成一个已设置好物品 Id 的掉落物；在 BeginPlay 前写入 Id，避免第一帧显示成默认物品。 */
    static ACandyFightItemPickup* SpawnItem(UWorld* World, FName ItemId, const FVector& Location,
        TSubclassOf<ACandyFightItemPickup> Class = nullptr, AActor* Owner = nullptr, float ArmDelay = .5f);

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    double ReadyTime = 0.;
    FText PromptKey = NSLOCTEXT("CandyFightItemUI", "DefaultPickupKey", "E 拾取");
    FVector BaseMeshLocation = FVector::ZeroVector;
    void RefreshPresentation();
    void ApplyColor();
};
