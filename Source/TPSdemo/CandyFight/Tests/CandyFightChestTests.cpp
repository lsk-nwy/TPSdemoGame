#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Interaction/CandyFightChest.h"
#include "CandyFight/Interaction/CandyFightSearchComponent.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "AIController.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "CandyFight/UI/CandyFightChestInteractionWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Blueprint/WidgetTree.h"
#include <limits>

namespace {
struct FChestWorld {
    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
    FChestWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->CreateAISystem(); }
    ~FChestWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    void Advance(float Seconds) { for (int32 I=0; I<FMath::CeilToInt(Seconds/.05f); ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); } }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightChestInteractionTest,"CandyFight.Chests.SearchAndWallet",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightChestInteractionTest::RunTest(const FString& Parameters)
{
    TGuardValue<uint64> RestoreFrame(GFrameCounter,GFrameCounter); FChestWorld F;
    auto* Class = LoadClass<ACandyFightCharacter>(nullptr,TEXT("/Game/CandyFight/Blueprints/BP_CandyFightCharacter.BP_CandyFightCharacter_C"));
    auto* Player = F.World->SpawnActor<ACandyFightCharacter>(Class,FVector(0,0,88),FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player asset"),Player)) return false;
    auto* PC = F.World->SpawnActor<APlayerController>();
    auto* Wallet = F.World->SpawnActor<ACandyFightPlayerState>(); Wallet->InitialGold=7; Wallet->DispatchBeginPlay(); PC->SetPlayerState(Wallet); PC->Possess(Player);
    Player->DispatchBeginPlay();
    auto* Config = NewObject<UCandyFightChestConfig>(); Config->SearchSeconds=.25f;
    auto* Chest = F.World->SpawnActor<ACandyFightChest>(FVector(140,0,0),FRotator::ZeroRotator);
    Chest->Config=Config; Chest->LootSeed=123; Chest->DispatchBeginPlay();
    const int32 Reward = Chest->Reward;
    TestTrue(TEXT("Reward range"),Reward>=10 && Reward<=100);
    auto& WeaponState=Player->Combat->WeaponStates[Player->Combat->EquippedIndex];
    --WeaponState.Ammo; WeaponState.BeginReload(*Player->Combat->GetCurrentConfig(),F.World->GetTimeSeconds());
    TestFalse(TEXT("Reload rejects start"),Player->Search->TryBeginSearch()); WeaponState.CancelReload();
    TestTrue(TEXT("Start near chest"),Player->Search->TryBeginSearch());
    TestFalse(TEXT("Combat blocked"),Player->IsCombatShotAllowed());
    TestFalse(TEXT("Weapon switch blocked"),Player->Combat->EquipTestWeapon(2));
    Player->Combat->Reload(); TestFalse(TEXT("Reload blocked during search"),Player->Combat->IsReloading());
    TestTrue(TEXT("Movement stopped"),Player->GetVelocity().IsNearlyZero());
    F.Advance(.1f); TestTrue(TEXT("World time progress"),Player->Search->GetSearchProgress()>0.f);
    auto* Enemy=F.World->SpawnActor<ACandyFightEnemyCharacter>(FVector(-1000,0,88),FRotator::ZeroRotator);
    auto* AI=F.World->SpawnActor<AAIController>(); AI->Possess(Enemy);
    TestEqual(TEXT("Actual enemy damage"),Player->TakeDamage(5.f,FDamageEvent(),AI,nullptr),5.f);
    TestFalse(TEXT("Damage cancels"),Player->Search->bSearching);
    TestTrue(TEXT("Combat restored"),Player->IsCombatShotAllowed());
    TestEqual(TEXT("Cancellation resets"),Player->Search->GetSearchProgress(),0.f);
    TestEqual(TEXT("Cancel retains reward"),Chest->Reward,Reward);
    TestEqual(TEXT("Cancel awards nothing"),Wallet->Gold,7);
    TestTrue(TEXT("Explicit restart"),Player->Search->TryBeginSearch());
    F.World->GetWorldSettings()->SetPauserPlayerState(Wallet);
    F.Advance(.5f); TestEqual(TEXT("Pause freezes progress"),Player->Search->GetSearchProgress(),0.f);
    F.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    F.Advance(.4f);
    TestFalse(TEXT("Completion unlocks"),Player->Search->bSearching);
    TestEqual(TEXT("Completed chest"),Chest->State,ECandyFightChestState::Searched);
    TestEqual(TEXT("Award once"),Wallet->Gold,7+Reward);
    Chest->Tick(.4f); TestTrue(TEXT("Lid opens upward"),Chest->Lid->GetComponentLocation().Z>60.f);
    TestFalse(TEXT("Repeated completion rejected"),Chest->CompleteSearch(Player));
    TestFalse(TEXT("Empty chest excluded"),Player->Search->TryBeginSearch());
    auto* Other = F.World->SpawnActor<ACandyFightChest>(FVector(0,140,0),FRotator::ZeroRotator); Other->DispatchBeginPlay();
    TestTrue(TEXT("Second chest starts"),Player->Search->TryBeginSearch()); Other->Destroy(); Player->Search->UpdateSearch();
    TestFalse(TEXT("Destroyed target unlocks"),Player->Search->bSearching);
    auto* Far = F.World->SpawnActor<ACandyFightChest>(FVector(500,0,0),FRotator::ZeroRotator); Far->DispatchBeginPlay();
    TestFalse(TEXT("Distance rejects"),Far->IsInReach(Player));
    Far->SetActorLocation(FVector(0,140,400)); TestFalse(TEXT("Height rejects"),Far->IsInReach(Player));
    Far->SetActorLocation(FVector(0,140,0));
    TestTrue(TEXT("Third target starts"),Player->Search->TryBeginSearch()); PC->UnPossess();
    TestFalse(TEXT("Unpossess unlocks"),Player->Search->bSearching); PC->Possess(Player);
    Player->PawnClientRestart(); TestEqual(TEXT("Restart preserves wallet"),Wallet->Gold,7+Reward);
    TestEqual(TEXT("Nonpositive award ignored"),Wallet->AddGold(-2),0);
    Wallet->Gold=MAX_int32-1; TestEqual(TEXT("Overflow capped"),Wallet->AddGold(100),1);
    Wallet->Gold=42;
    auto* HUDClass=LoadClass<UCandyFightWeaponHUD>(nullptr,TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightWeaponHUD.BP_CandyFightWeaponHUD_C"));
    auto* HUD=NewObject<UCandyFightWeaponHUD>(F.World,HUDClass); HUD->Initialize(); HUD->TakeWidget(); HUD->Connect(Player->Combat);
    TestNotNull(TEXT("Designer GoldText binding"),HUD->GoldText.Get());
    if(HUD->GoldText) TestTrue(TEXT("HUD reads wallet"),HUD->GoldText->GetText().ToString().Contains(FString::FromInt(Wallet->Gold)));
    HUD->Connect(Player->Combat); TestEqual(TEXT("Unique currency binding"),Wallet->OnGoldChanged.GetAllObjects().Num(),1);
    HUD->Connect(nullptr); TestEqual(TEXT("Disconnect unbinds"),Wallet->OnGoldChanged.GetAllObjects().Num(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightChestDistributionTest,"CandyFight.Chests.ConfigurationAndDistribution",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightChestDistributionTest::RunTest(const FString& Parameters)
{
    auto* Config=NewObject<UCandyFightChestConfig>(); FString Error;
    TestTrue(TEXT("Default config valid"),Config->Validate(Error));
    Config->SearchSeconds=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("NaN rejected"),Config->Validate(Error)); Config->SearchSeconds=5;
    Config->MaxGold=9; TestFalse(TEXT("Reverse rewards rejected"),Config->Validate(Error)); Config->MaxGold=100;
    TArray<FVector> First;
    for(int32 Run=0;Run<2;++Run) {
        FChestWorld F;
        auto* Override=NewObject<UCandyFightChestConfig>(); Override->MinGold=42; Override->MaxGold=42;
        for(int32 I=0;I<8;++I) {
            FActorSpawnParameters Params; Params.Name=FName(*FString::Printf(TEXT("Point%d"),I));
            auto* P=F.World->SpawnActor<ACandyFightChestSpawnPoint>(FVector(I*500,0,0),FRotator::ZeroRotator,Params);
            P->Region=I<4 ? TEXT("A") : TEXT("B"); P->ConfigOverride=Override;
        }
        auto* Manager=F.World->SpawnActor<ACandyFightChestSpawner>(); Manager->bUseFixedSeed=true; Manager->RandomSeed=99;
        FCandyFightChestRegion A; A.Region=TEXT("A"); A.Count=2; FCandyFightChestRegion B; B.Region=TEXT("B"); B.Count=2;
        Manager->Regions={A,B}; Manager->GenerateChests();
        TestEqual(TEXT("Two per region"),Manager->SpawnedChests.Num(),4); TSet<FVector> Unique;
        for(int32 I=0;I<Manager->SpawnedChests.Num();++I) {
            auto* C=Manager->SpawnedChests[I].Get(); Unique.Add(C->GetActorLocation()); TestEqual(TEXT("Point config override"),C->Config.Get(),Override);
            if(Run==0) First.Add(C->GetActorLocation()); else TestEqual(TEXT("Fixed seed reproducible"),C->GetActorLocation(),First[I]);
        }
        TestEqual(TEXT("No duplicate positions"),Unique.Num(),4); Manager->GenerateChests(); TestEqual(TEXT("Generate only once"),Manager->SpawnedChests.Num(),4);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightChestWidgetTest,"CandyFight.Chests.IndependentWidget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightChestWidgetTest::RunTest(const FString& Parameters)
{
    FChestWorld F;
    auto* Class=LoadClass<UCandyFightChestInteractionWidget>(nullptr,TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightChestInteractionWidget.BP_CandyFightChestInteractionWidget_C"));
    if(!TestNotNull(TEXT("Independent designer widget"),Class)) return false;
    auto* Widget=NewObject<UCandyFightChestInteractionWidget>(F.World,Class); Widget->Initialize(); Widget->TakeWidget();
    TestNotNull(TEXT("Ring binding"),Widget->SearchRing.Get());
    if(Widget->SearchRing) TestNotNull(TEXT("UI material instance"),Widget->SearchRing->GetDynamicMaterial());
    Widget->SetDisplay(ECandyFightChestPrompt::Looted,1.f,FText::FromString(TEXT("E")),42);
    if(Widget->PromptText) TestEqual(TEXT("Reward text"),Widget->PromptText->GetText().ToString(),FString(TEXT("+42 金币")));
    Widget->SetDisplay(ECandyFightChestPrompt::Ready,0.f,FText::FromString(TEXT("F")),0);
    if(Widget->KeyText) TestEqual(TEXT("Configured key"),Widget->KeyText->GetText().ToString(),FString(TEXT("F")));
    return true;
}
#endif
