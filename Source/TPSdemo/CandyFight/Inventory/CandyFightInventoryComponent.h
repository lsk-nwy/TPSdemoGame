#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CandyFight/Inventory/CandyFightItem.h"
#include "CandyFightInventoryComponent.generated.h"

class ACharacter;
class APlayerController;
class ACandyFightItemPickup;
class UCandyFightInventoryWidget;
class UEnhancedInputComponent;
class UInputComponent;

/** 背包格子。演示版不堆叠：一个格子最多放一件物品。 */
USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FName ItemId;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 Count = 0;

    bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCandyFightInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightInventoryToggled, bool, bOpen);

/**
 * 玩家背包：默认 6×6，共 36 格；每件物品占一格，不堆叠。
 * 负责拾取、使用、丢弃以及背包界面的暂停与输入模式。
 */
UCLASS(ClassGroup=(CandyFight), meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCandyFightInventoryComponent();

    /** 列数与行数可在蓝图里修改；界面按 6×6 布局。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="12"))
    int32 GridColumns = 6;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="12"))
    int32 GridRows = 6;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Pickup", meta=(ClampMin="0", Units="cm"))
    float PickupDistance = 220.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Pickup", meta=(ClampMin="0", Units="cm"))
    float PickupHeightDifference = 140.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="cm"))
    float DropDistance = 140.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="cm"))
    float DropHeight = 35.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|UI")
    TSubclassOf<UCandyFightInventoryWidget> InventoryWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Pickup")
    TSubclassOf<ACandyFightItemPickup> PickupClass;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory")
    TArray<FCandyFightInventorySlot> Slots;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory|UI")
    bool bInventoryOpen = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory|Pickup")
    TObjectPtr<ACandyFightItemPickup> FocusedPickup;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Inventory|UI")
    TObjectPtr<UCandyFightInventoryWidget> InventoryWidget;

    UPROPERTY(BlueprintAssignable, Category="Inventory")
    FCandyFightInventoryChanged OnInventoryChanged;

    UPROPERTY(BlueprintAssignable, Category="Inventory")
    FCandyFightInventoryToggled OnInventoryToggled;

    /** 返回放入的格子下标；物品未知或背包已满时返回 INDEX_NONE。 */
    UFUNCTION(BlueprintCallable, Category="Inventory")
    int32 AddItem(FName ItemId);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool RemoveAt(int32 SlotIndex);

    /** 使用成功后才消耗物品；失败时 OutMessage 说明原因。 */
    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool UseAt(int32 SlotIndex, FText& OutMessage);

    /** 在角色前方地面生成可再次拾取的物品；生成成功后才消耗格子。 */
    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool DropAt(int32 SlotIndex, FText& OutMessage);

    UFUNCTION(BlueprintPure, Category="Inventory")
    bool GetItemAt(int32 SlotIndex, FCandyFightItemDefinition& OutItem) const;

    UFUNCTION(BlueprintPure, Category="Inventory")
    FName GetItemIdAt(int32 SlotIndex) const;

    UFUNCTION(BlueprintPure, Category="Inventory")
    int32 GetSlotCount() const { return Slots.Num(); }

    UFUNCTION(BlueprintPure, Category="Inventory")
    int32 GetUsedSlotCount() const;

    UFUNCTION(BlueprintPure, Category="Inventory")
    bool IsFull() const;

    UFUNCTION(BlueprintPure, Category="Inventory|UI")
    bool IsInventoryOpen() const { return bInventoryOpen; }

    UFUNCTION(BlueprintCallable, Category="Inventory|UI")
    bool OpenInventory();

    UFUNCTION(BlueprintCallable, Category="Inventory|UI")
    bool CloseInventory();

    UFUNCTION(BlueprintCallable, Category="Inventory|UI")
    bool ToggleInventory();

    UFUNCTION(BlueprintCallable, Category="Inventory|Pickup")
    bool TryPickupFocused();

    UFUNCTION(BlueprintCallable, Category="Inventory|Pickup")
    void RefreshFocusedPickup();

    /** 立即结算限时效果；供测试或不驱动组件 tick 的场景使用。 */
    UFUNCTION(BlueprintCallable, Category="Inventory")
    void UpdateTimedEffects();

    /** 由角色在本地玩家接管时调用；不注册新的映射资产，只复用交互键和 B 键。 */
    void InitializeLocalPlayer();
    void ReleaseLocalPlayer();
    void BindInput(UEnhancedInputComponent* Input);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    friend class FCandyFightInventoryTest;
    friend class FCandyFightInventoryWidgetTest;

    TWeakObjectPtr<ACharacter> SpeedBuffTarget;
    float SpeedBuffBase = 0.f;
    float SpeedBuffMultiplier = 1.f;
    double SpeedBuffEndTime = 0.;

    UPROPERTY(Transient)
    TObjectPtr<UInputComponent> InventoryInput;
    TWeakObjectPtr<APlayerController> InputOwner;

    void EnsureGrid();
    void SetFocusedPickup(ACandyFightItemPickup* NewFocus);
    void ApplySpeedEffect(ACharacter& Character, const FCandyFightItemDefinition& Item);
    void UpdateSpeedEffect(double Now);
    void EndSpeedEffect();
    void Notify(const FText& Message) const;
    void HandleInteract();
    void HandleToggleKey();
    void RemoveInventoryWidget();
};
