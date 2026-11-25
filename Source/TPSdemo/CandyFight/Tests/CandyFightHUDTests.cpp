#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "CandyFight/CandyFightGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightHUDDisplayTest, "CandyFight.HUD.DesignerAndDisplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightHUDDisplayTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<UCandyFightWeaponHUD>(nullptr,
        TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightWeaponHUD.BP_CandyFightWeaponHUD_C"));
    if (!TestNotNull(TEXT("HUD Blueprint loads"), Class)) return false;
    auto* HUD = NewObject<UCandyFightWeaponHUD>(GetTransientPackage(), Class);
    TestTrue(TEXT("HUD initializes"), HUD->Initialize());
    TestNotNull(TEXT("Designer root is Canvas"), Cast<UCanvasPanel>(HUD->WidgetTree->RootWidget));
    for (const TCHAR* Name : {TEXT("WeaponName"), TEXT("AmmoText"), TEXT("StatusText"), TEXT("ReloadProgress"),
        TEXT("HealthBar"), TEXT("HealthText"), TEXT("CountdownText")})
        TestNotNull(*FString::Printf(TEXT("Designer widget %s exists"), Name), HUD->GetWidgetFromName(Name));
    if (!TestNotNull(TEXT("Health bar binds"), HUD->HealthBar.Get()) ||
        !TestNotNull(TEXT("Health text binds"), HUD->HealthText.Get()) ||
        !TestNotNull(TEXT("Countdown binds"), HUD->CountdownText.Get())) return false;
    TestEqual(TEXT("Full health preview"), HUD->HealthBar->GetPercent(), 1.f);
    TestEqual(TEXT("Four minute preview"), HUD->CountdownText->GetText().ToString(), FString(TEXT("04:00")));
    HUD->SetHealthDisplay(35, 100);
    TestEqual(TEXT("Health ratio"), HUD->HealthBar->GetPercent(), .35f);
    TestEqual(TEXT("Health label"), HUD->HealthText->GetText().ToString(), FString(TEXT("35 / 100")));
    HUD->SetHealthDisplay(150, 100);
    TestEqual(TEXT("Health capped"), HUD->HealthBar->GetPercent(), 1.f);
    HUD->SetHealthDisplay(-1, 100);
    TestEqual(TEXT("Negative health clamped"), HUD->HealthBar->GetPercent(), 0.f);
    HUD->SetHealthDisplay(100, 0);
    TestEqual(TEXT("Invalid max gives empty bar"), HUD->HealthBar->GetPercent(), 0.f);
    HUD->SetHealthDisplay(std::numeric_limits<float>::quiet_NaN(), 100);
    TestEqual(TEXT("Nonfinite health gives empty bar"), HUD->HealthBar->GetPercent(), 0.f);
    for (const TCHAR* Name : {TEXT("WeaponPanel"), TEXT("HealthPanel"), TEXT("TimerPanel")})
    {
        auto* Panel = HUD->GetWidgetFromName(Name);
        if (!TestNotNull(TEXT("Panel exists"), Panel)) continue;
        auto* Slot = Cast<UCanvasPanelSlot>(Panel->Slot);
        if (!TestNotNull(TEXT("Panel has editable Canvas slot"), Slot)) continue;
        const FVector2D CustomPosition(53, 71);
        Slot->SetPosition(CustomPosition);
        HUD->SetHealthDisplay(80, 100);
        HUD->Connect(nullptr);
        TestEqual(TEXT("Data refresh preserves Designer position"), Slot->GetPosition(), CustomPosition);
    }
    auto* Native = NewObject<UCandyFightWeaponHUD>();
    Native->Initialize();
    Native->TakeWidget();
    TestNotNull(TEXT("Native fallback health"), Native->HealthBar.Get());
    TestNotNull(TEXT("Native fallback timer"), Native->CountdownText.Get());
    Native->ReleaseSlateResources(true);
    HUD->ReleaseSlateResources(true);
    TestEqual(TEXT("Start"), UCandyFightWeaponHUD::FormatCountdown(240).ToString(), FString(TEXT("04:00")));
    TestEqual(TEXT("Round up"), UCandyFightWeaponHUD::FormatCountdown(239.1f).ToString(), FString(TEXT("04:00")));
    TestEqual(TEXT("Minute boundary"), UCandyFightWeaponHUD::FormatCountdown(60).ToString(), FString(TEXT("01:00")));
    TestEqual(TEXT("Last fraction"), UCandyFightWeaponHUD::FormatCountdown(.01f).ToString(), FString(TEXT("00:01")));
    TestEqual(TEXT("End"), UCandyFightWeaponHUD::FormatCountdown(-1).ToString(), FString(TEXT("00:00")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightHUDCountdownTest, "CandyFight.HUD.CountdownLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightHUDCountdownTest::RunTest(const FString& Parameters)
{
    TGuardValue<uint64> RestoreFrame(GFrameCounter, GFrameCounter);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Mode = World->SpawnActor<ACandyFightGameMode>();
    Mode->GameState = World->SpawnActor<AGameStateBase>();
    World->SetGameState(Mode->GameState);
    TestEqual(TEXT("Default duration"), Mode->GetRemainingCountdownSeconds(), 240.f);
    Mode->CountdownDuration = 2.f;
    Mode->StartPlay();
    TestEqual(TEXT("Starts at configured duration"), Mode->GetRemainingCountdownSeconds(), 2.f);
    // Keep individual frames below the engine's maximum simulation delta.
    const auto Advance = [&](int32 Frames)
    {
        for (int32 Index = 0; Index < Frames; ++Index)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, .25f);
        }
    };
    Advance(3);
    TestTrue(TEXT("World time counts down"), FMath::IsNearlyEqual(Mode->GetRemainingCountdownSeconds(), 1.25f));
    World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
    Advance(2);
    TestTrue(TEXT("Pause freezes countdown"), FMath::IsNearlyEqual(Mode->GetRemainingCountdownSeconds(), 1.25f));
    World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    Mode->StartPlay();
    TestTrue(TEXT("Repeated StartPlay cannot restart"), FMath::IsNearlyEqual(Mode->GetRemainingCountdownSeconds(), 1.25f));
    // UI lifecycle must not own or reset the game clock.
    auto* HUD = NewObject<UCandyFightWeaponHUD>();
    HUD->Initialize(); HUD->TakeWidget(); HUD->ReleaseSlateResources(true);
    TestTrue(TEXT("HUD rebuild leaves clock intact"), FMath::IsNearlyEqual(Mode->GetRemainingCountdownSeconds(), 1.25f));
    Advance(8);
    Mode->Tick(0.f);
    TestEqual(TEXT("Clamped at zero"), Mode->GetRemainingCountdownSeconds(), 0.f);
    TestFalse(TEXT("Completion stops ticking"), Mode->IsActorTickEnabled());
    Advance(4);
    Mode->Tick(0.f);
    TestEqual(TEXT("Stays at zero"), Mode->GetRemainingCountdownSeconds(), 0.f);
    auto* NewMode = World->SpawnActor<ACandyFightGameMode>();
    NewMode->GameState = Mode->GameState;
    NewMode->StartPlay();
    TestEqual(TEXT("New game starts fresh"), NewMode->GetRemainingCountdownSeconds(), 240.f);
    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
