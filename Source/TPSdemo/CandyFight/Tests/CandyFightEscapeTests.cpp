#include "CandyFightEscapeTestObserver.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/CandyFightGameMode.h"
void UCandyFightEscapeTestObserver::Escaped(ACandyFightCharacter* Player)
{
    ++EscapedCount;
    bStateCommitted=Player && Player->GetPlayerState<ACandyFightPlayerState>() && Player->GetPlayerState<ACandyFightPlayerState>()->bHasEscaped;
}
void UCandyFightEscapeTestObserver::Countdown() { ++CountdownCount; bCountdownCommitted=Mode && Mode->GetRemainingCountdownSeconds()==0.f; }

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Interaction/CandyFightEscapeGate.h"
#include "CandyFight/Interaction/CandyFightEscapeComponent.h"
#include "CandyFight/Interaction/CandyFightChest.h"
#include "CandyFight/Interaction/CandyFightSearchComponent.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/UI/CandyFightEscapeGateWidget.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "AIController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include <limits>
namespace
{
struct FEscapeWorld
{
    TGuardValue<uint64> Frame{GFrameCounter,GFrameCounter};
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ACandyFightCharacter* Player=nullptr;
    APlayerController* PC=nullptr;
    ACandyFightPlayerState* State=nullptr;
    FEscapeWorld()
    {
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->CreateAISystem();
        auto* Class=LoadClass<ACandyFightCharacter>(nullptr,TEXT("/Game/CandyFight/Blueprints/BP_CandyFightCharacter.BP_CandyFightCharacter_C"));
        Player=World->SpawnActor<ACandyFightCharacter>(Class,FVector(0,0,96),FRotator::ZeroRotator);
        PC=World->SpawnActor<APlayerController>(); State=World->SpawnActor<ACandyFightPlayerState>(); State->DispatchBeginPlay();
        Player->MaxHealth=100.f; // Isolate this fixture from authored Blueprint health settings.
        PC->SetPlayerState(State); PC->Possess(Player); Player->DispatchBeginPlay();
        Player->GetCharacterMovement()->SetComponentTickEnabled(false);
    }
    ~FEscapeWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    void Advance(float Seconds)
    {
        const int32 Frames=FMath::RoundToInt(Seconds/.05f);
        for (int32 I=0;I<Frames;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
    }
    ACandyFightEscapeGate* Gate(FVector Position=FVector::ZeroVector,float Open=10.f,float Wait=.5f)
    {
        auto* Gate=World->SpawnActor<ACandyFightEscapeGate>(Position,FRotator::ZeroRotator);
        Gate->OpenSeconds=Open; Gate->EscapeSeconds=Wait; Gate->DispatchBeginPlay(); return Gate;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEscapeLifecycleTest,"CandyFight.Escape.OccupancyAndSuccess",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightEscapeLifecycleTest::RunTest(const FString& Parameters)
{
    FEscapeWorld F; auto* Gate=F.Gate(); auto* Observer=NewObject<UCandyFightEscapeTestObserver>(F.World);
    Gate->OnPlayerEscaped.AddDynamic(Observer,&UCandyFightEscapeTestObserver::Escaped);
    F.Player->Escape->UpdateEscape(); TestTrue(TEXT("Entry starts automatically"),F.Player->Escape->bEscaping);
    TestTrue(TEXT("Escape allows combat"),F.Player->IsCombatShotAllowed());
    TestTrue(TEXT("Escape allows weapon switch"),F.Player->Combat->EquipTestWeapon(2));
    auto& WeaponState=F.Player->Combat->WeaponStates[F.Player->Combat->EquippedIndex]; --WeaponState.Ammo;
    // This headless world has no focused local viewport; exercise reload state directly.
    TestTrue(TEXT("Reload state remains available during escape"),WeaponState.BeginReload(*F.Player->Combat->GetCurrentConfig(),F.World->GetTimeSeconds()));
    TestTrue(TEXT("Reload is active while waiting"),F.Player->Combat->IsReloading()); WeaponState.CancelReload();
    auto* Chest=F.World->SpawnActor<ACandyFightChest>(FVector(140,0,0),FRotator::ZeroRotator); Chest->DispatchBeginPlay();
    TestTrue(TEXT("Chest search can start during escape"),F.Player->Search->TryBeginSearch());
    F.Advance(.15f); const float Before=F.Player->Escape->GetEscapeProgress();
    TestTrue(TEXT("Escape progresses while searching"),Before>0.f && F.Player->Search->bSearching);
    auto* Enemy=F.World->SpawnActor<ACandyFightEnemyCharacter>(FVector(-1000,0,96),FRotator::ZeroRotator);
    auto* AI=F.World->SpawnActor<AAIController>(); AI->Possess(Enemy);
    TestEqual(TEXT("Lethal enemy damage"),F.Player->TakeDamage(1000.f,FDamageEvent(),AI,nullptr),100.f);
    TestEqual(TEXT("Damage preserves progress"),F.Player->Escape->GetEscapeProgress(),Before);
    TestFalse(TEXT("Damage cancels chest search independently"),F.Player->Search->bSearching);
    TestTrue(TEXT("Zero health continues waiting"),F.Player->Escape->bEscaping);
    F.Player->SetActorLocation(FVector(300,0,96)); F.Player->Escape->UpdateEscape();
    TestFalse(TEXT("Leave cancels"),F.Player->Escape->bEscaping); TestEqual(TEXT("Leave clears progress"),F.Player->Escape->GetEscapeProgress(),0.f);
    F.Player->SetActorLocation(FVector(0,0,96)); F.Player->Escape->UpdateEscape();
    F.World->GetWorldSettings()->SetPauserPlayerState(F.State); F.Advance(.5f);
    TestEqual(TEXT("Paused wait freezes"),F.Player->Escape->GetEscapeProgress(),0.f);
    TestEqual(TEXT("Paused gate freezes"),Gate->GetRemainingCloseSeconds(),9.85f);
    F.World->GetWorldSettings()->SetPauserPlayerState(nullptr); F.Advance(.6f);
    TestTrue(TEXT("Success stored in PlayerState at zero health"),F.State->bHasEscaped);
    TestEqual(TEXT("Success event once"),Observer->EscapedCount,1); TestTrue(TEXT("State before callback"),Observer->bStateCommitted);
    TestFalse(TEXT("Success clears waiting"),F.Player->Escape->bEscaping); TestTrue(TEXT("Success leaves gate open"),Gate->IsOpen());
    TestFalse(TEXT("Duplicate completion rejected"),Gate->CompleteEscape(F.Player));
    F.Player->PawnClientRestart(); F.Player->Escape->UpdateEscape(); TestTrue(TEXT("Restart preserves success"),F.State->bHasEscaped);
    auto* NewState=F.World->SpawnActor<ACandyFightPlayerState>(); NewState->DispatchBeginPlay(); TestFalse(TEXT("New round starts unescaped"),NewState->bHasEscaped);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEscapeSelectionTest,"CandyFight.Escape.RangeSelectionAndClosure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightEscapeSelectionTest::RunTest(const FString& Parameters)
{
    FEscapeWorld F; auto* A=F.Gate({50,0,0},10,2); auto* B=F.Gate({100,0,0},10,2);
    F.Player->Escape->UpdateEscape(); TestEqual(TEXT("Nearest chosen"),F.Player->Escape->TargetGate.Get(),A);
    F.Advance(.1f); F.Player->SetActorLocation({90,0,96}); F.Player->Escape->UpdateEscape();
    TestEqual(TEXT("Closer overlapping gate does not steal target"),F.Player->Escape->TargetGate.Get(),A);
    A->CloseGate(); F.Player->Escape->UpdateEscape(); TestEqual(TEXT("Closed target switches automatically"),F.Player->Escape->TargetGate.Get(),B);
    TestEqual(TEXT("Replacement starts from zero"),F.Player->Escape->GetEscapeProgress(),0.f);
    B->Destroy(); F.Player->Escape->UpdateEscape(); TestFalse(TEXT("Destroy clears target"),F.Player->Escape->bEscaping);
    auto* C=F.Gate({90,0,0},10,2); F.Player->Escape->UpdateEscape(); F.PC->UnPossess();
    TestFalse(TEXT("Control loss cancels immediately"),F.Player->Escape->bEscaping); F.PC->Possess(F.Player);
    F.Player->SetActorLocation({90,0,400}); TestFalse(TEXT("Capsule feet height checked"),C->CanEscape(F.Player));
    F.Player->SetActorLocation({300,0,96}); TestFalse(TEXT("Horizontal radius checked"),C->CanEscape(F.Player));
    F.Player->SetActorLocation({0,0,96});
    auto* Wall=F.World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Wall->AddInstanceComponent(Box); Box->SetBoxExtent({10,100,100}); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Wall->SetActorLocation({45,0,50});
    TestFalse(TEXT("Wall prevents occupancy"),C->CanEscape(F.Player)); Wall->Destroy();
    TestTrue(TEXT("Unobstructed reach"),C->CanEscape(F.Player)); C->CloseGate();
    auto* Equal=F.Gate({0,0,0},.5f,.5f); auto* Observer=NewObject<UCandyFightEscapeTestObserver>(F.World);
    Equal->OnClosed.AddDynamic(Observer,&UCandyFightEscapeTestObserver::Closed); Equal->OnPlayerEscaped.AddDynamic(Observer,&UCandyFightEscapeTestObserver::Escaped);
    F.Player->Escape->UpdateEscape(); F.Advance(.5f); Equal->Tick(0); F.Player->Escape->UpdateEscape();
    TestFalse(TEXT("Closing wins completion frame"),F.State->bHasEscaped); TestEqual(TEXT("No success at deadline"),Observer->EscapedCount,0);
    Equal->CloseGate(); Equal->CloseGate(); TestEqual(TEXT("Close event once"),Observer->ClosedCount,1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEscapeClockTest,"CandyFight.Escape.IndependentClocksAndGlobalEvent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightEscapeClockTest::RunTest(const FString& Parameters)
{
    FEscapeWorld F; F.Player->SetActorLocation({1000,0,96}); auto* A=F.Gate({},2,1);
    F.Advance(.5f); auto* B=F.Gate({0,500,0},2,1);
    TestEqual(TEXT("Spawn starts independent clock"),B->GetRemainingCloseSeconds(),2.f);
    A->OpenSeconds=100; A->EscapeRadius=2000; A->EscapeSeconds=50;
    TestEqual(TEXT("Duration snapshotted"),A->GetEscapeDuration(),1.f); TestFalse(TEXT("Radius snapshotted"),A->CanEscape(F.Player));
    auto* Mode=F.World->SpawnActor<ACandyFightGameMode>(); Mode->GameState=F.World->SpawnActor<AGameStateBase>(); F.World->SetGameState(Mode->GameState);
    Mode->CountdownDuration=.25f; auto* Observer=NewObject<UCandyFightEscapeTestObserver>(F.World); Observer->Mode=Mode;
    Mode->OnCountdownFinished.AddDynamic(Observer,&UCandyFightEscapeTestObserver::Countdown); Mode->StartPlay(); F.Advance(.3f); Mode->Tick(0);
    TestEqual(TEXT("Global event once"),Observer->CountdownCount,1); TestTrue(TEXT("Global state committed before callback"),Observer->bCountdownCommitted);
    TestTrue(TEXT("Global expiry leaves gates running"),A->IsOpen() && B->IsOpen()); TestTrue(TEXT("Global expiry leaves combat enabled"),F.Player->IsCombatShotAllowed());
    auto* HUD=NewObject<UCandyFightWeaponHUD>(F.World); HUD->Initialize(); HUD->TakeWidget(); HUD->ReleaseSlateResources(true); HUD->TakeWidget();
    Mode->StartPlay(); Mode->Tick(0); TestEqual(TEXT("HUD rebuild/start cannot repeat global event"),Observer->CountdownCount,1);
    TestEqual(TEXT("Global display fixed at zero"),UCandyFightWeaponHUD::FormatCountdown(Mode->GetRemainingCountdownSeconds()).ToString(),FString(TEXT("00:00")));
    F.Advance(1.3f); TestFalse(TEXT("First gate expires"),A->IsOpen()); TestTrue(TEXT("Later gate remains open"),B->IsOpen());
    auto* Invalid=F.World->SpawnActor<ACandyFightEscapeGate>(); Invalid->EscapeSeconds=0;
    FString Error; TestFalse(TEXT("Invalid duration validation"),Invalid->ValidateConfiguration(Error));
    AddExpectedError(TEXT("disabled:"),EAutomationExpectedErrorFlags::Contains,1); Invalid->DispatchBeginPlay(); TestFalse(TEXT("Invalid gate remains closed"),Invalid->IsOpen());
    Invalid->EscapeSeconds=5; Invalid->EscapeRadius=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite range rejected"),Invalid->ValidateConfiguration(Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEscapeWidgetTest,"CandyFight.Escape.DesignerWidgetAndPrefab",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightEscapeWidgetTest::RunTest(const FString& Parameters)
{
    FEscapeWorld F;
    auto* Class=LoadClass<UCandyFightEscapeGateWidget>(nullptr,TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightEscapeGateWidget.BP_CandyFightEscapeGateWidget_C"));
    if (!TestNotNull(TEXT("Independent Widget Blueprint"),Class)) return false;
    auto* Widget=NewObject<UCandyFightEscapeGateWidget>(F.World,Class); Widget->Initialize(); Widget->TakeWidget();
    if (!TestNotNull(TEXT("Timer binding"),Widget->CountdownText.Get()) || !TestNotNull(TEXT("Progress binding"),Widget->EscapeProgress.Get())) return false;
    auto* Slot=Cast<UCanvasPanelSlot>(Widget->CountdownText->Slot); Slot->SetPosition({18,21});
    auto Font=Widget->CountdownText->GetFont(); Font.Size=31; Widget->CountdownText->SetFont(Font);
    Widget->SetDisplay(true,119.1f,true,.6f,2.1f,false);
    TestEqual(TEXT("Timer rounds up"),Widget->CountdownText->GetText().ToString(),FString(TEXT("02:00")));
    TestEqual(TEXT("Progress reflects logic"),Widget->EscapeProgress->GetPercent(),.6f);
    TestEqual(TEXT("Editable layout retained"),Slot->GetPosition(),FVector2D(18,21)); TestEqual(TEXT("Editable font retained"),Widget->CountdownText->GetFont().Size,31.f);
    Widget->SetDisplay(true,100,false,0,0,true); TestEqual(TEXT("Success label"),Widget->TitleText->GetText().ToString(),FString(TEXT("逃生成功")));
    Widget->SetDisplay(false,0,false,0,0,false); TestEqual(TEXT("Closed label"),Widget->TitleText->GetText().ToString(),FString(TEXT("已关闭")));
    TestEqual(TEXT("Closed hides progress"),Widget->EscapeProgress->GetVisibility(),ESlateVisibility::Hidden);
    Widget->ReleaseSlateResources(true); Widget->TakeWidget(); TestEqual(TEXT("Rebuild preserves layout"),Slot->GetPosition(),FVector2D(18,21));
    auto* Native=NewObject<UCandyFightEscapeGateWidget>(F.World); Native->Initialize(); Native->TakeWidget(); TestNotNull(TEXT("Native fallback"),Native->CountdownText.Get());
    auto* GateClass=LoadClass<ACandyFightEscapeGate>(nullptr,TEXT("/Game/CandyFight/Blueprints/Interaction/BP_CandyFightEscapeGate.BP_CandyFightEscapeGate_C"));
    if (!TestNotNull(TEXT("Prefab loads"),GateClass)) return false;
    auto* Gate=F.World->SpawnActor<ACandyFightEscapeGate>(GateClass,FVector(500,0,0),FRotator::ZeroRotator); Gate->DispatchBeginPlay();
    TestEqual(TEXT("Disc nonblocking"),Gate->Disc->GetCollisionEnabled(),ECollisionEnabled::NoCollision); TestFalse(TEXT("Disc does not affect nav"),Gate->Disc->CanEverAffectNavigation());
    TestEqual(TEXT("Screen space"),Gate->GateWidget->GetWidgetSpace(),EWidgetSpace::Screen); TestEqual(TEXT("Prefab independent widget"),Gate->GateWidget->GetWidgetClass().Get(),Class);
    const float Remaining=Gate->GetRemainingCloseSeconds(); Gate->GateWidget->SetWidgetClass(nullptr); Gate->GateWidget->SetWidgetClass(Class); Gate->GateWidget->InitWidget();
    TestEqual(TEXT("Widget rebuild leaves clock"),Gate->GetRemainingCloseSeconds(),Remaining);
    return true;
}
#endif
