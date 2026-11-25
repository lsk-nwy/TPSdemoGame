#include "CandyFight/Inventory/CandyFightInventoryComponent.h"
#include "CandyFight/Inventory/CandyFightItemPickup.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/Flow/CandyFightFlow.h"
#include "CandyFight/Interaction/CandyFightSearchComponent.h"
#include "CandyFight/UI/CandyFightInventoryWidget.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "Blueprint/UserWidget.h"
#include "CollisionQueryParams.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TPSdemo.h"

UCandyFightInventoryComponent::UCandyFightInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    Slots.SetNum(GridColumns * GridRows);
}

void UCandyFightInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureGrid();
    if (!PickupClass) PickupClass = ACandyFightItemPickup::StaticClass();
}

void UCandyFightInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bInventoryOpen) CloseInventory();
    else RemoveInventoryWidget();
    EndSpeedEffect();
    ReleaseLocalPlayer();
    Super::EndPlay(EndPlayReason);
}

void UCandyFightInventoryComponent::EnsureGrid()
{
    GridColumns = FMath::Clamp(GridColumns, 1, 12);
    GridRows = FMath::Clamp(GridRows, 1, 12);
    const int32 Desired = GridColumns * GridRows;
    if (Slots.Num() == Desired) return;
    const int32 OldCount = Slots.Num();
    Slots.SetNum(Desired);
    // SetNum 新增元素默认就是空格；这里显式写清楚，避免以后改结构体默认值。
    for (int32 Index = OldCount; Index < Desired; ++Index)
    {
        Slots[Index].ItemId = NAME_None;
        Slots[Index].Count = 0;
    }
}

int32 UCandyFightInventoryComponent::AddItem(FName ItemId)
{
    EnsureGrid();
    FCandyFightItemDefinition Item;
    if (!UCandyFightItemLibrary::FindItem(ItemId, Item)) return INDEX_NONE;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (!Slots[Index].IsEmpty()) continue;
        Slots[Index].ItemId = ItemId;
        Slots[Index].Count = 1;
        OnInventoryChanged.Broadcast();
        return Index;
    }
    return INDEX_NONE;
}

bool UCandyFightInventoryComponent::RemoveAt(int32 SlotIndex)
{
    EnsureGrid();
    if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty()) return false;
    Slots[SlotIndex].ItemId = NAME_None;
    Slots[SlotIndex].Count = 0;
    OnInventoryChanged.Broadcast();
    return true;
}

bool UCandyFightInventoryComponent::GetItemAt(int32 SlotIndex, FCandyFightItemDefinition& OutItem) const
{
    OutItem = FCandyFightItemDefinition();
    if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty()) return false;
    return UCandyFightItemLibrary::FindItem(Slots[SlotIndex].ItemId, OutItem);
}

FName UCandyFightInventoryComponent::GetItemIdAt(int32 SlotIndex) const
{
    return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].ItemId : NAME_None;
}

int32 UCandyFightInventoryComponent::GetUsedSlotCount() const
{
    int32 Used = 0;
    for (const FCandyFightInventorySlot& Slot : Slots) if (!Slot.IsEmpty()) ++Used;
    return Used;
}

bool UCandyFightInventoryComponent::IsFull() const
{
    if (Slots.Num() == 0) return true;
    for (const FCandyFightInventorySlot& Slot : Slots) if (Slot.IsEmpty()) return false;
    return true;
}

bool UCandyFightInventoryComponent::UseAt(int32 SlotIndex, FText& OutMessage)
{
    EnsureGrid();
    if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty())
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "UseEmpty", "该格子没有物品");
        return false;
    }
    FCandyFightItemDefinition Item;
    if (!UCandyFightItemLibrary::FindItem(Slots[SlotIndex].ItemId, Item))
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "UseUnknown", "物品数据无效");
        return false;
    }
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    if (!Player)
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "UseNoPlayer", "无法找到玩家角色");
        return false;
    }

    bool bApplied = false;
    FText Message;
    switch (Item.Effect)
    {
    case ECandyFightItemEffect::Heal:
        if (Player->Health >= Player->MaxHealth)
        {
            OutMessage = NSLOCTEXT("CandyFightInventory", "UseHealthFull", "生命值已满，无法使用");
            return false;
        }
        {
            const float Restored = Player->Heal(Item.EffectAmount);
            if (Restored <= 0.f)
            {
                OutMessage = NSLOCTEXT("CandyFightInventory", "UseHealNoEffect", "生命值已满，无法使用");
                return false;
            }
            FFormatNamedArguments Args;
            Args.Add(TEXT("Item"), Item.DisplayName);
            Args.Add(TEXT("Amount"), FMath::RoundToInt(Restored));
            Message = FText::Format(NSLOCTEXT("CandyFightInventory", "UseHeal", "使用 {Item}，恢复 {Amount} 点生命"), Args);
            bApplied = true;
        }
        break;
    case ECandyFightItemEffect::Speed:
        if (!FMath::IsFinite(Item.EffectAmount) || Item.EffectAmount <= 0.f ||
            !FMath::IsFinite(Item.EffectDuration) || Item.EffectDuration <= 0.f ||
            !Player->GetCharacterMovement())
        {
            OutMessage = NSLOCTEXT("CandyFightInventory", "UseSpeedInvalid", "物品效果无效");
            return false;
        }
        ApplySpeedEffect(*Player, Item);
        {
            FFormatNamedArguments Args;
            Args.Add(TEXT("Item"), Item.DisplayName);
            Args.Add(TEXT("Percent"), FMath::RoundToInt(Item.EffectAmount * 100.f));
            Message = FText::Format(NSLOCTEXT("CandyFightInventory", "UseSpeed", "使用 {Item}：移动速度 +{Percent}%"), Args);
            bApplied = true;
        }
        break;
    case ECandyFightItemEffect::Gold:
    {
        auto* Wallet = Player->GetPlayerState<ACandyFightPlayerState>();
        const int32 Amount = FMath::RoundToInt(Item.EffectAmount);
        if (!Wallet || Amount <= 0 || Wallet->AddGold(Amount) <= 0)
        {
            OutMessage = NSLOCTEXT("CandyFightInventory", "UseGoldFailed", "无法获得金币");
            return false;
        }
        FFormatNamedArguments Args;
        Args.Add(TEXT("Item"), Item.DisplayName);
        Args.Add(TEXT("Amount"), Amount);
        Message = FText::Format(NSLOCTEXT("CandyFightInventory", "UseGold", "使用 {Item}，获得 {Amount} 金币"), Args);
        bApplied = true;
        break;
    }
    case ECandyFightItemEffect::None:
    default:
    {
        FFormatNamedArguments Args;
        Args.Add(TEXT("Item"), Item.DisplayName);
        OutMessage = FText::Format(NSLOCTEXT("CandyFightInventory", "UseMaterial", "{Item} 是任务材料，无法使用"), Args);
        return false;
    }
    }

    if (!bApplied) return false;
    Slots[SlotIndex].ItemId = NAME_None;
    Slots[SlotIndex].Count = 0;
    OnInventoryChanged.Broadcast();
    Notify(Message);
    OutMessage = Message;
    return true;
}

bool UCandyFightInventoryComponent::DropAt(int32 SlotIndex, FText& OutMessage)
{
    EnsureGrid();
    if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty())
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "DropEmpty", "该格子没有物品");
        return false;
    }
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!Player || !World)
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "DropNoPlayer", "无法丢弃物品");
        return false;
    }

    FCandyFightItemDefinition Item;
    if (!UCandyFightItemLibrary::FindItem(Slots[SlotIndex].ItemId, Item))
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "DropUnknown", "物品数据无效");
        return false;
    }

    FVector Forward = Player->GetActorForwardVector();
    Forward.Z = 0.f;
    Forward = Forward.GetSafeNormal();
    if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
    const FVector Target = Player->GetActorLocation() + Forward * FMath::Max(0.f, DropDistance);
    FVector SpawnLocation = Target;
    FHitResult GroundHit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightInventoryDrop), false, Player);
    if (World->LineTraceSingleByChannel(GroundHit, Target + FVector(0.f, 0.f, 120.f),
        Target - FVector(0.f, 0.f, 320.f), ECC_Visibility, Params))
    {
        SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, FMath::Max(0.f, DropHeight));
    }
    else
    {
        SpawnLocation.Z -= 40.f;
    }

    TSubclassOf<ACandyFightItemPickup> SpawnClass = PickupClass;
    if (!SpawnClass) SpawnClass = ACandyFightItemPickup::StaticClass();
    auto* Pickup = ACandyFightItemPickup::SpawnItem(World, Slots[SlotIndex].ItemId, SpawnLocation,
        SpawnClass, Player, .75f);
    if (!Pickup)
    {
        OutMessage = NSLOCTEXT("CandyFightInventory", "DropSpawnFailed", "无法放置物品");
        return false;
    }

    Slots[SlotIndex].ItemId = NAME_None;
    Slots[SlotIndex].Count = 0;
    OnInventoryChanged.Broadcast();
    FFormatNamedArguments Args;
    Args.Add(TEXT("Item"), Item.DisplayName);
    OutMessage = FText::Format(NSLOCTEXT("CandyFightInventory", "Dropped", "丢弃 {Item}"), Args);
    Notify(OutMessage);
    return true;
}

void UCandyFightInventoryComponent::ApplySpeedEffect(ACharacter& Character, const FCandyFightItemDefinition& Item)
{
    auto* Movement = Character.GetCharacterMovement();
    UWorld* World = GetWorld();
    if (!Movement || !World) return;
    const double Now = World->GetTimeSeconds();
    if (SpeedBuffTarget.Get() != &Character || Now >= SpeedBuffEndTime)
    {
        SpeedBuffBase = Movement->MaxWalkSpeed;
    }
    SpeedBuffMultiplier = FMath::Max(1.f, 1.f + Item.EffectAmount);
    Movement->MaxWalkSpeed = SpeedBuffBase * SpeedBuffMultiplier;
    SpeedBuffTarget = &Character;
    SpeedBuffEndTime = Now + Item.EffectDuration;
}

void UCandyFightInventoryComponent::UpdateSpeedEffect(double Now)
{
    if (!SpeedBuffTarget.IsValid()) return;
    if (Now < SpeedBuffEndTime) return;
    EndSpeedEffect();
}

void UCandyFightInventoryComponent::EndSpeedEffect()
{
    if (auto* Character = SpeedBuffTarget.Get())
    {
        if (auto* Movement = Character->GetCharacterMovement())
        {
            Movement->MaxWalkSpeed = SpeedBuffBase;
        }
    }
    SpeedBuffTarget.Reset();
    SpeedBuffBase = 0.f;
    SpeedBuffMultiplier = 1.f;
    SpeedBuffEndTime = 0.;
}

void UCandyFightInventoryComponent::UpdateTimedEffects()
{
    if (GetWorld()) UpdateSpeedEffect(GetWorld()->GetTimeSeconds());
}

void UCandyFightInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetWorld()) return;
    UpdateTimedEffects();
    RefreshFocusedPickup();
}

void UCandyFightInventoryComponent::BindInput(UEnhancedInputComponent* Input)
{
    if (!Input) return;
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner()); Player && Player->Search && Player->Search->InteractAction)
    {
        Input->BindAction(Player->Search->InteractAction, ETriggerEvent::Started, this, &ThisClass::HandleInteract);
    }
}

void UCandyFightInventoryComponent::InitializeLocalPlayer()
{
    ReleaseLocalPlayer();
    // 本地玩家重新接管后，旧的暂停界面不能留在屏幕上。
    if (bInventoryOpen) CloseInventory();
    RefreshFocusedPickup();

    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    auto* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController()) return;

    // UEnhancedInputComponent 删除了 BindKey；B 用独立的高优先级输入组件，和原有的 ESC/Tab 流程分开。
    InventoryInput = NewObject<UInputComponent>(PC, TEXT("CandyFightInventoryInput"));
    if (!InventoryInput) return;
    InventoryInput->RegisterComponent();
    InventoryInput->Priority = 20;
    InventoryInput->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::HandleToggleKey);
    PC->PushInputComponent(InventoryInput);
    InputOwner = PC;
}

void UCandyFightInventoryComponent::ReleaseLocalPlayer()
{
    if (InputOwner.IsValid() && InventoryInput)
    {
        InputOwner->PopInputComponent(InventoryInput);
    }
    if (InventoryInput)
    {
        InventoryInput->DestroyComponent();
        InventoryInput = nullptr;
    }
    InputOwner.Reset();
}

void UCandyFightInventoryComponent::HandleInteract()
{
    if (bInventoryOpen || UGameplayStatics::IsGamePaused(this)) return;
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner()); Player && Player->Search && Player->Search->bSearching) return;
    TryPickupFocused();
}

void UCandyFightInventoryComponent::HandleToggleKey()
{
    ToggleInventory();
}

bool UCandyFightInventoryComponent::ToggleInventory()
{
    return bInventoryOpen ? CloseInventory() : OpenInventory();
}

bool UCandyFightInventoryComponent::OpenInventory()
{
    if (bInventoryOpen || !GetWorld()) return false;
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    auto* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController()) return false;
    if (UGameplayStatics::IsGamePaused(this)) return false;
    if (Player->Health <= 0.f) return false;
    if (auto* Flow = Cast<UCandyFightGameInstance>(GetWorld()->GetGameInstance());
        Flow && (Flow->Screen != ECandyFightFlowScreen::Battle || !Flow->IsBattleActive())) return false;

    EnsureGrid();
    RemoveInventoryWidget();
    const TSubclassOf<UCandyFightInventoryWidget> WidgetClass = InventoryWidgetClass
        ? InventoryWidgetClass.Get() : UCandyFightInventoryWidget::StaticClass();
    InventoryWidget = CreateWidget<UCandyFightInventoryWidget>(PC, WidgetClass);
    if (!InventoryWidget) return false;
    InventoryWidget->SetInventory(this);
    InventoryWidget->AddToPlayerScreen(800);

    if (Player->Combat) Player->Combat->StopFire();
    if (Player->Search) Player->Search->CancelSearch();
    Player->ConsumeMovementInputVector();
    if (auto* Movement = Player->GetCharacterMovement()) Movement->StopMovementImmediately();
    SetFocusedPickup(nullptr);

    bInventoryOpen = true;
    if (!UGameplayStatics::SetGamePaused(this, true))
    {
        bInventoryOpen = false;
        RemoveInventoryWidget();
        return false;
    }

    if (PC->PlayerInput) PC->PlayerInput->FlushPressedKeys();
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;
    InventoryWidget->SetKeyboardFocus();
    OnInventoryToggled.Broadcast(true);
    UE_LOG(LogTPSdemo, Display, TEXT("[Inventory] Opened %dx%d (%d/%d used)"),
        GridColumns, GridRows, GetUsedSlotCount(), GetSlotCount());
    return true;
}

bool UCandyFightInventoryComponent::CloseInventory()
{
    if (!bInventoryOpen) return false;
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    auto* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
    RemoveInventoryWidget();
    bInventoryOpen = false;

    auto* Flow = GetWorld() ? Cast<UCandyFightGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
    const bool bBattleScreen = !Flow || Flow->Screen == ECandyFightFlowScreen::Battle;
    if (bBattleScreen)
    {
        UGameplayStatics::SetGamePaused(this, false);
        if (PC && PC->IsLocalController())
        {
            if (PC->PlayerInput) PC->PlayerInput->FlushPressedKeys();
            FInputModeGameOnly InputMode;
            InputMode.SetConsumeCaptureMouseDown(false);
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = false;
            if (auto* LocalPlayer = PC->GetLocalPlayer(); LocalPlayer && LocalPlayer->ViewportClient)
            {
                LocalPlayer->ViewportClient->SetMouseLockMode(EMouseLockMode::LockAlways);
                LocalPlayer->ViewportClient->SetHideCursorDuringCapture(true);
            }
        }
    }

    OnInventoryToggled.Broadcast(false);
    RefreshFocusedPickup();
    UE_LOG(LogTPSdemo, Display, TEXT("[Inventory] Closed."));
    return true;
}

void UCandyFightInventoryComponent::RemoveInventoryWidget()
{
    if (!InventoryWidget) return;
    InventoryWidget->SetInventory(nullptr);
    InventoryWidget->RemoveFromParent();
    InventoryWidget = nullptr;
}

void UCandyFightInventoryComponent::RefreshFocusedPickup()
{
    if (bInventoryOpen || !GetWorld() || UGameplayStatics::IsGamePaused(this))
    {
        SetFocusedPickup(nullptr);
        return;
    }
    if (auto* Flow = Cast<UCandyFightGameInstance>(GetWorld()->GetGameInstance());
        Flow && Flow->Screen != ECandyFightFlowScreen::Battle)
    {
        SetFocusedPickup(nullptr);
        return;
    }
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    if (!Player || Player->Health <= 0.f)
    {
        SetFocusedPickup(nullptr);
        return;
    }

    const double MaxDistance = FMath::Max(0.f, PickupDistance);
    const double MaxDistanceSq = MaxDistance * MaxDistance;
    const double MaxHeight = FMath::Max(0.f, PickupHeightDifference);
    ACandyFightItemPickup* Nearest = nullptr;
    double NearestDistance = TNumericLimits<double>::Max();
    for (TActorIterator<ACandyFightItemPickup> It(GetWorld()); It; ++It)
    {
        ACandyFightItemPickup* Pickup = *It;
        if (!IsValid(Pickup) || !Pickup->IsPickupReady()) continue;
        const FVector Delta = Pickup->GetActorLocation() - Player->GetActorLocation();
        if (FMath::Abs(Delta.Z) > MaxHeight) continue;
        const double DistanceSq = Delta.SizeSquared2D();
        if (DistanceSq > MaxDistanceSq || DistanceSq >= NearestDistance) continue;
        Nearest = Pickup;
        NearestDistance = DistanceSq;
    }
    SetFocusedPickup(Nearest);
}

void UCandyFightInventoryComponent::SetFocusedPickup(ACandyFightItemPickup* NewFocus)
{
    if (NewFocus && NewFocus == FocusedPickup) return;
    if (!NewFocus && !FocusedPickup) return;
    if (IsValid(FocusedPickup)) FocusedPickup->SetFocused(false);
    FocusedPickup = NewFocus;
    if (IsValid(FocusedPickup))
    {
        if (auto* Player = Cast<ACandyFightCharacter>(GetOwner()))
        {
            FocusedPickup->SetPromptKey(Player->Search ? Player->Search->GetInteractKeyText() : FText::FromString(TEXT("E")));
        }
        FocusedPickup->SetFocused(true);
    }
}

bool UCandyFightInventoryComponent::TryPickupFocused()
{
    if (bInventoryOpen) return false;
    RefreshFocusedPickup();
    if (!IsValid(FocusedPickup)) return false;
    if (IsFull())
    {
        Notify(NSLOCTEXT("CandyFightInventory", "PickupFull", "背包已满（6×6），无法拾取"));
        return false;
    }
    const FName ItemId = FocusedPickup->ItemId;
    FCandyFightItemDefinition Item;
    if (!UCandyFightItemLibrary::FindItem(ItemId, Item)) return false;
    const int32 SlotIndex = AddItem(ItemId);
    if (SlotIndex == INDEX_NONE) return false;

    ACandyFightItemPickup* Picked = FocusedPickup;
    FocusedPickup = nullptr;
    Picked->SetFocused(false);
    Picked->Consume();
    FFormatNamedArguments Args;
    Args.Add(TEXT("Item"), Item.DisplayName);
    Args.Add(TEXT("Used"), GetUsedSlotCount());
    Args.Add(TEXT("Total"), GetSlotCount());
    Notify(FText::Format(NSLOCTEXT("CandyFightInventory", "PickedUp", "拾取 {Item}（背包 {Used}/{Total}）"), Args));
    return true;
}

void UCandyFightInventoryComponent::Notify(const FText& Message) const
{
    UE_LOG(LogTPSdemo, Display, TEXT("[Inventory] %s"), *Message.ToString());
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner()))
    {
        if (auto* PC = Cast<APlayerController>(Player->GetController()); PC && PC->IsLocalController())
        {
            PC->ClientMessage(Message.ToString());
        }
    }
}
