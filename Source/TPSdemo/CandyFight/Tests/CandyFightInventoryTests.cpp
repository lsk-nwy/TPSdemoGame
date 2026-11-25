#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/Inventory/CandyFightInventoryComponent.h"
#include "CandyFight/Inventory/CandyFightItem.h"
#include "CandyFight/Inventory/CandyFightItemPickup.h"
#include "CandyFight/UI/CandyFightInventoryWidget.h"
#include "CandyFight/UI/CandyFightItemPickupWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    struct FInventoryWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        FInventoryWorld()
        {
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->CreateAISystem();
        }
        ~FInventoryWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
        void Advance(float Seconds)
        {
            for (int32 Index = 0; Index < FMath::CeilToInt(Seconds / .05f); ++Index)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, .05f);
            }
        }
        ACandyFightCharacter* SpawnPlayer()
        {
            auto* Class = LoadClass<ACandyFightCharacter>(nullptr,
                TEXT("/Game/CandyFight/Blueprints/BP_CandyFightCharacter.BP_CandyFightCharacter_C"));
            auto* Player = Class
                ? World->SpawnActor<ACandyFightCharacter>(Class, FVector(1000, 0, 88), FRotator::ZeroRotator)
                : nullptr;
            if (!Player) return nullptr;
            auto* PC = World->SpawnActor<APlayerController>();
            auto* Wallet = World->SpawnActor<ACandyFightPlayerState>();
            Wallet->InitialGold = 7;
            Wallet->DispatchBeginPlay();
            PC->SetPlayerState(Wallet);
            PC->Possess(Player);
            Player->MaxHealth = 100.f;
            Player->DispatchBeginPlay();
            Player->Health = 100.f;
            return Player;
        }
    };

    void DestroyPickups(UWorld* World)
    {
        for (TActorIterator<ACandyFightItemPickup> It(World); It; ++It) It->Destroy();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightInventoryCatalogTest, "CandyFight.Inventory.Catalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightInventoryCatalogTest::RunTest(const FString& Parameters)
{
    const TArray<FCandyFightItemDefinition> Catalog = UCandyFightItemLibrary::GetCatalog();
    TestTrue(TEXT("At least five test items"), Catalog.Num() >= 5);
    TSet<FName> Ids;
    for (const FCandyFightItemDefinition& Item : Catalog)
    {
        TestFalse(TEXT("Item id is set"), Item.Id.IsNone());
        TestFalse(TEXT("Item name is set"), Item.DisplayName.IsEmpty());
        TestFalse(TEXT("Item description is set"), Item.Description.IsEmpty());
        TestFalse(TEXT("Duplicate item id"), Ids.Contains(Item.Id));
        Ids.Add(Item.Id);
        FCandyFightItemDefinition Found;
        TestTrue(TEXT("FindItem finds every catalog entry"), UCandyFightItemLibrary::FindItem(Item.Id, Found));
        TestEqual(TEXT("FindItem returns the same id"), Found.Id.ToString(), Item.Id.ToString());
    }
    TestEqual(TEXT("Test item list covers catalog"), UCandyFightItemLibrary::GetTestItemIds().Num(), Catalog.Num());
    FCandyFightItemDefinition Missing;
    TestFalse(TEXT("FindItem rejects unknown id"), UCandyFightItemLibrary::FindItem(TEXT("MissingItem"), Missing));
    TestFalse(TEXT("FindItem rejects none"), UCandyFightItemLibrary::FindItem(NAME_None, Missing));

    FCandyFightItemDefinition Mint;
    TestTrue(TEXT("Mint exists"), UCandyFightItemLibrary::FindItem(TEXT("MintCandy"), Mint));
    TestTrue(TEXT("Mint is usable"), UCandyFightItemLibrary::IsUsable(Mint));
    TestTrue(TEXT("Mint effect is described"), UCandyFightItemLibrary::DescribeEffect(Mint).ToString().Contains(TEXT("25")));
    FCandyFightItemDefinition Caramel;
    TestTrue(TEXT("Caramel exists"), UCandyFightItemLibrary::FindItem(TEXT("CaramelChunk"), Caramel));
    TestFalse(TEXT("Caramel is not usable"), UCandyFightItemLibrary::IsUsable(Caramel));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightInventoryGridTest, "CandyFight.Inventory.GridPickupUseDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightInventoryGridTest::RunTest(const FString& Parameters)
{
    TGuardValue<uint64> RestoreFrame(GFrameCounter, GFrameCounter);
    FInventoryWorld Fixture;
    auto* Player = Fixture.SpawnPlayer();
    if (!TestNotNull(TEXT("Player Blueprint"), Player) || !TestNotNull(TEXT("Inventory component"), Player->Inventory.Get())) return false;
    auto* Inventory = Player->Inventory.Get();
    auto* Wallet = Player->GetPlayerState<ACandyFightPlayerState>();
    if (!TestNotNull(TEXT("Wallet"), Wallet)) return false;

    TestEqual(TEXT("6x6 grid"), Inventory->GetSlotCount(), 36);
    TestEqual(TEXT("First item goes to slot 0"), Inventory->AddItem(TEXT("MintCandy")), 0);
    TestEqual(TEXT("Duplicate item uses a new slot"), Inventory->AddItem(TEXT("MintCandy")), 1);
    TestEqual(TEXT("Two separate cells"), Inventory->GetUsedSlotCount(), 2);
    TestEqual(TEXT("Unknown item rejected"), Inventory->AddItem(TEXT("MissingItem")), INDEX_NONE);
    TestEqual(TEXT("Empty id rejected"), Inventory->AddItem(NAME_None), INDEX_NONE);
    FCandyFightItemDefinition Found;
    TestTrue(TEXT("Item definition lookup"), Inventory->GetItemAt(1, Found));
    TestEqual(TEXT("Slot 1 definition"), Found.Id.ToString(), FString(TEXT("MintCandy")));
    TestEqual(TEXT("Slot id helper"), Inventory->GetItemIdAt(1).ToString(), FString(TEXT("MintCandy")));
    TestFalse(TEXT("Empty slot lookup"), Inventory->GetItemAt(2, Found));
    TestFalse(TEXT("Remove empty slot"), Inventory->RemoveAt(2));
    TestFalse(TEXT("Remove invalid slot"), Inventory->RemoveAt(99));
    TestTrue(TEXT("Remove item"), Inventory->RemoveAt(0));
    TestTrue(TEXT("Slot is empty"), Inventory->Slots[0].IsEmpty());
    TestEqual(TEXT("First free cell reused"), Inventory->AddItem(TEXT("CaramelChunk")), 0);

    for (int32 Index = 0; Index < 36; ++Index) Inventory->RemoveAt(Index);
    for (int32 Index = 0; Index < 36; ++Index)
    {
        TestEqual(TEXT("Fill cells in order"), Inventory->AddItem(TEXT("CaramelChunk")), Index);
    }
    TestEqual(TEXT("Full inventory rejected"), Inventory->AddItem(TEXT("MintCandy")), INDEX_NONE);
    TestTrue(TEXT("Inventory reports full"), Inventory->IsFull());
    for (int32 Index = 0; Index < 36; ++Index) Inventory->RemoveAt(Index);
    TestEqual(TEXT("All cells cleared"), Inventory->GetUsedSlotCount(), 0);

    Player->Health = 40.f;
    const int32 HealSlot = Inventory->AddItem(TEXT("MintCandy"));
    FText Message;
    TestTrue(TEXT("Use heal item"), Inventory->UseAt(HealSlot, Message));
    TestEqual(TEXT("Heal amount applied"), Player->Health, 65.f);
    TestTrue(TEXT("Use message names item"), Message.ToString().Contains(TEXT("薄荷糖")));
    TestTrue(TEXT("Used cell is empty"), Inventory->Slots[HealSlot].IsEmpty());
    TestFalse(TEXT("Use empty cell"), Inventory->UseAt(HealSlot, Message));

    Player->Health = Player->MaxHealth;
    const int32 FullHealthSlot = Inventory->AddItem(TEXT("MintCandy"));
    TestFalse(TEXT("Full health blocks heal"), Inventory->UseAt(FullHealthSlot, Message));
    TestFalse(TEXT("Blocked use keeps item"), Inventory->Slots[FullHealthSlot].IsEmpty());
    TestTrue(TEXT("Full health message"), Message.ToString().Contains(TEXT("满")));
    Inventory->RemoveAt(FullHealthSlot);

    const int32 MaterialSlot = Inventory->AddItem(TEXT("CaramelChunk"));
    TestFalse(TEXT("Material cannot be used"), Inventory->UseAt(MaterialSlot, Message));
    TestFalse(TEXT("Material stays in inventory"), Inventory->Slots[MaterialSlot].IsEmpty());
    Inventory->RemoveAt(MaterialSlot);

    const int32 GoldSlot = Inventory->AddItem(TEXT("GoldTicket"));
    TestTrue(TEXT("Use gold ticket"), Inventory->UseAt(GoldSlot, Message));
    TestEqual(TEXT("Gold added"), Wallet->Gold, 37);
    TestTrue(TEXT("Gold cell emptied"), Inventory->Slots[GoldSlot].IsEmpty());

    auto* Movement = Player->GetCharacterMovement();
    Movement->MaxWalkSpeed = 500.f;
    const int32 SpeedSlot = Inventory->AddItem(TEXT("EnergyDrink"));
    TestTrue(TEXT("Use speed item"), Inventory->UseAt(SpeedSlot, Message));
    TestTrue(TEXT("Speed increased"), Movement->MaxWalkSpeed > 500.f);
    Fixture.Advance(8.5f);
    // 测试世界不会自动驱动组件 tick，显式结算一次限时效果。
    Inventory->UpdateTimedEffects();
    TestTrue(TEXT("Speed restored after duration"), FMath::IsNearlyEqual(Movement->MaxWalkSpeed, 500.f));

    DestroyPickups(Fixture.World);
    const int32 DropSlot = Inventory->AddItem(TEXT("CaramelChunk"));
    FText DropMessage;
    TestTrue(TEXT("Drop item"), Inventory->DropAt(DropSlot, DropMessage));
    TestTrue(TEXT("Dropped cell emptied"), Inventory->Slots[DropSlot].IsEmpty());
    ACandyFightItemPickup* Dropped = nullptr;
    for (TActorIterator<ACandyFightItemPickup> It(Fixture.World); It; ++It)
    {
        if (It->ItemId == FName(TEXT("CaramelChunk"))) Dropped = *It;
    }
    TestNotNull(TEXT("Dropped pickup exists"), Dropped);

    DestroyPickups(Fixture.World);
    auto* Near = ACandyFightItemPickup::SpawnItem(Fixture.World, TEXT("EmergencyKit"),
        Player->GetActorLocation() + FVector(100.f, 0.f, 0.f), nullptr, nullptr, 0.f);
    if (TestNotNull(TEXT("Spawn near pickup"), Near))
    {
        Near->DispatchBeginPlay();
        Inventory->RefreshFocusedPickup();
        TestTrue(TEXT("Focus finds nearest pickup"), Inventory->FocusedPickup.Get() == Near);
        const int32 BeforePickup = Inventory->GetUsedSlotCount();
        TestTrue(TEXT("Pickup succeeds"), Inventory->TryPickupFocused());
        TestEqual(TEXT("Pickup fills one cell"), Inventory->GetUsedSlotCount(), BeforePickup + 1);
        TestTrue(TEXT("Picked up actor is destroyed"), Near->IsActorBeingDestroyed());
        TestTrue(TEXT("Focus cleared after pickup"), Inventory->FocusedPickup == nullptr);
    }

    for (int32 Index = 0; Index < 36; ++Index)
    {
        if (Inventory->Slots[Index].IsEmpty()) Inventory->AddItem(TEXT("CaramelChunk"));
    }
    TestTrue(TEXT("Inventory filled for pickup rejection"), Inventory->IsFull());
    DestroyPickups(Fixture.World);
    auto* Blocked = ACandyFightItemPickup::SpawnItem(Fixture.World, TEXT("EmergencyKit"),
        Player->GetActorLocation() + FVector(100.f, 0.f, 0.f), nullptr, nullptr, 0.f);
    if (TestNotNull(TEXT("Spawn blocked pickup"), Blocked))
    {
        Blocked->DispatchBeginPlay();
        Inventory->RefreshFocusedPickup();
        TestFalse(TEXT("Full inventory rejects pickup"), Inventory->TryPickupFocused());
        TestFalse(TEXT("Blocked pickup remains"), Blocked->IsActorBeingDestroyed());
    }

    DestroyPickups(Fixture.World);
    auto* Far = ACandyFightItemPickup::SpawnItem(Fixture.World, TEXT("EmergencyKit"),
        Player->GetActorLocation() + FVector(1000.f, 0.f, 0.f), nullptr, nullptr, 0.f);
    if (TestNotNull(TEXT("Spawn far pickup"), Far))
    {
        Far->DispatchBeginPlay();
        Inventory->RefreshFocusedPickup();
        TestTrue(TEXT("Out of range pickup is not focused"), Inventory->FocusedPickup == nullptr);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightInventoryWidgetTest, "CandyFight.Inventory.Widget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightInventoryWidgetTest::RunTest(const FString& Parameters)
{
    FInventoryWorld Fixture;
    auto* Player = Fixture.SpawnPlayer();
    if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Inventory"), Player->Inventory.Get())) return false;
    auto* Inventory = Player->Inventory.Get();

    auto* Widget = NewObject<UCandyFightInventoryWidget>(Fixture.World);
    TestTrue(TEXT("Widget initializes"), Widget->Initialize());
    Widget->TakeWidget();
    Widget->SetInventory(Inventory);
    TestEqual(TEXT("36 slot buttons"), Widget->SlotButtons.Num(), 36);
    TestNotNull(TEXT("Capacity label"), Widget->CapacityText.Get());
    TestNotNull(TEXT("Selected name label"), Widget->SelectedNameText.Get());
    TestNotNull(TEXT("Use button"), Widget->UseButton.Get());
    TestNotNull(TEXT("Drop button"), Widget->DropButton.Get());

    const int32 Slot = Inventory->AddItem(TEXT("MintCandy"));
    Widget->Refresh();
    if (Widget->CapacityText) TestEqual(TEXT("Capacity text"), Widget->CapacityText->GetText().ToString(), FString(TEXT("已使用 1 / 36")));
    Widget->SelectSlot(Slot);
    TestEqual(TEXT("Selection stored"), Widget->GetSelectedSlot(), Slot);
    if (Widget->SlotButtons.IsValidIndex(0) && Widget->SlotButtons[0])
    {
        Widget->SelectSlot(1);
        Widget->SlotButtons[0]->OnClicked.Broadcast();
        TestEqual(TEXT("Slot button selects its cell"), Widget->GetSelectedSlot(), 0);
        Widget->SelectSlot(Slot);
    }
    if (Widget->SelectedNameText) TestEqual(TEXT("Selected item name"), Widget->SelectedNameText->GetText().ToString(), FString(TEXT("薄荷糖")));
    if (Widget->SlotTexts.IsValidIndex(Slot) && Widget->SlotTexts[Slot])
        TestEqual(TEXT("Slot label"), Widget->SlotTexts[Slot]->GetText().ToString(), FString(TEXT("薄荷糖")));
    if (Widget->UseButton)
    {
        Player->Health = 40.f;
        TestTrue(TEXT("Use button enabled"), Widget->UseButton->GetIsEnabled());
        Widget->UseButton->OnClicked.Broadcast();
        TestEqual(TEXT("Widget use applies heal"), Player->Health, 65.f);
        TestTrue(TEXT("Widget use empties cell"), Inventory->Slots[Slot].IsEmpty());
        TestFalse(TEXT("Empty slot disables use"), Widget->UseButton->GetIsEnabled());
    }

    const int32 DropSlot = Inventory->AddItem(TEXT("CaramelChunk"));
    Widget->SelectSlot(DropSlot);
    if (Widget->DropButton)
    {
        TestTrue(TEXT("Drop button enabled"), Widget->DropButton->GetIsEnabled());
        Widget->DropButton->OnClicked.Broadcast();
        TestTrue(TEXT("Widget drop empties cell"), Inventory->Slots[DropSlot].IsEmpty());
    }

    auto* Prompt = NewObject<UCandyFightItemPickupWidget>(Fixture.World);
    TestTrue(TEXT("Prompt initializes"), Prompt->Initialize());
    Prompt->TakeWidget();
    Prompt->SetDisplay(FText::FromString(TEXT("薄荷糖")), FText::FromString(TEXT("E 拾取")));
    if (Prompt->ItemNameText) TestEqual(TEXT("Prompt item name"), Prompt->ItemNameText->GetText().ToString(), FString(TEXT("薄荷糖")));
    if (Prompt->PickupKeyText) TestEqual(TEXT("Prompt key"), Prompt->PickupKeyText->GetText().ToString(), FString(TEXT("E 拾取")));

    Widget->ReleaseSlateResources(true);
    Prompt->ReleaseSlateResources(true);
    return true;
}
#endif
