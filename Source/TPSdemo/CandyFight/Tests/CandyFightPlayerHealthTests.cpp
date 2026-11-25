#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFightPlayerHealthTestObserver.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/AI/CandyFightEnemyConfig.h"
#include "CandyFight/AI/CandyFightEnemyPerceptionComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "Components/ProgressBar.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include <limits>

namespace
{
struct FPlayerHealthWorld
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FPlayerHealthWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->CreateAISystem(); }
    ~FPlayerHealthWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    ACandyFightCharacter* SpawnPlayer()
    {
        auto* Class = LoadClass<ACandyFightCharacter>(nullptr,
            TEXT("/Game/CandyFight/Blueprints/BP_CandyFightCharacter.BP_CandyFightCharacter_C"));
        auto* Player = Class ? World->SpawnActor<ACandyFightCharacter>(Class, FVector(1000,0,88), FRotator::ZeroRotator) : nullptr;
        // Test numbers must not depend on the designer's current Blueprint defaults.
        if (Player) Player->MaxHealth = 100.f;
        return Player;
    }
    AAIController* SpawnEnemyController()
    {
        auto* Enemy = World->SpawnActor<ACandyFightEnemyCharacter>(FVector(-1000,0,88), FRotator::ZeroRotator);
        auto* AI = World->SpawnActor<AAIController>(); AI->Possess(Enemy); return AI;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPlayerHealthTest, "CandyFight.PlayerHealth.DamageAndHealing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightPlayerHealthTest::RunTest(const FString& Parameters)
{
    FPlayerHealthWorld Fixture;
    auto* Player = Fixture.SpawnPlayer();
    if (!TestNotNull(TEXT("Player Blueprint"), Player)) return false;
    auto* AI = Fixture.SpawnEnemyController();
    auto* PC = Fixture.World->SpawnActor<APlayerController>(); PC->Possess(Player);
    auto* OtherAI = Fixture.World->SpawnActor<AAIController>();
    auto* Observer = NewObject<UCandyFightPlayerHealthTestObserver>();
    Player->OnHealthChanged.AddDynamic(Observer, &UCandyFightPlayerHealthTestObserver::Changed);
    Player->OnPlayerDamaged.AddDynamic(Observer, &UCandyFightPlayerHealthTestObserver::Damaged);
    Player->MaxHealth = 150.f; Player->DispatchBeginPlay();
    TestEqual(TEXT("Configured full health"), Player->Health, 150.f);
    TestEqual(TEXT("Initialization is not damage"), Observer->DamageCount, 0);
    const int32 InitialChanges = Observer->ChangeCount;
    for (auto* Source : {static_cast<AController*>(PC), static_cast<AController*>(OtherAI), static_cast<AController*>(nullptr)})
        TestEqual(TEXT("Non-enemy damage ignored"), Player->TakeDamage(20, FDamageEvent(), Source, AI->GetPawn()), 0.f);
    for (float Invalid : {0.f, -20.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        TestEqual(TEXT("Invalid damage ignored"), Player->TakeDamage(Invalid, FDamageEvent(), AI, nullptr), 0.f);
        TestEqual(TEXT("Invalid healing ignored"), Player->Heal(Invalid), 0.f);
    }
    Player->SetCanBeDamaged(false);
    TestEqual(TEXT("Damage disabled respected"), Player->TakeDamage(20, FDamageEvent(), AI, nullptr), 0.f);
    Player->SetCanBeDamaged(true);
    TestEqual(TEXT("Ignored inputs do not notify"), Observer->ChangeCount, InitialChanges);
    TestEqual(TEXT("Unrepresentable tiny loss is not notified"), Player->TakeDamage(std::numeric_limits<float>::denorm_min(), FDamageEvent(), AI, nullptr), 0.f);
    FCandyFightProjectileDamageEvent Event; Event.CumulativeDamage = 999.f;
    TestEqual(TEXT("Hit delta used, not cumulative metadata"), Player->TakeDamage(20, Event, AI, nullptr), 20.f);
    TestEqual(TEXT("No invulnerability"), Player->TakeDamage(30, Event, AI, nullptr), 30.f);
    Player->PawnClientRestart();
    TestEqual(TEXT("Input restart preserves health"), Player->Health, 100.f);
    TestEqual(TEXT("Overkill capped to remaining health"), Player->TakeDamage(999, Event, AI, nullptr), 100.f);
    TestEqual(TEXT("Health zero"), Player->Health, 0.f);
    TestEqual(TEXT("Only actual hits notify"), Observer->DamageCount, 3);
    TestEqual(TEXT("Actual total damage"), Observer->TotalDamage, 150.f);
    TestEqual(TEXT("Zero health ignores later hits"), Player->TakeDamage(20, Event, AI, nullptr), 0.f);
    TestEqual(TEXT("No repeated zero notification"), Observer->DamageCount, 3);
    TestFalse(TEXT("Player still exists"), Player->IsActorBeingDestroyed());
    TestTrue(TEXT("Movement remains enabled"), Player->GetCharacterMovement()->MovementMode != MOVE_None);
    TestTrue(TEXT("Combat allowed at zero"), Player->IsCombatShotAllowed());
    TestTrue(TEXT("Combat continues ticking"), Player->Combat->IsComponentTickEnabled());
    TestEqual(TEXT("Healing from zero"), Player->Heal(35), 35.f);
    TestEqual(TEXT("Healing capped"), Player->Heal(999), 115.f);
    const int32 FullChanges = Observer->ChangeCount;
    TestEqual(TEXT("Full health healing has no effect"), Player->Heal(20), 0.f);
    TestEqual(TEXT("Full healing does not notify"), Observer->ChangeCount, FullChanges);
    TestEqual(TEXT("Healing never emits damage"), Observer->DamageCount, 3);
    auto* NewPlayer = Fixture.SpawnPlayer();
    NewPlayer->MaxHealth = std::numeric_limits<float>::quiet_NaN(); NewPlayer->DispatchBeginPlay();
    TestEqual(TEXT("Invalid max falls back"), NewPlayer->MaxHealth, 100.f);
    TestEqual(TEXT("New player starts full"), NewPlayer->Health, 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPlayerHealthHUDTest, "CandyFight.PlayerHealth.HUDLifecycleAndFlash",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightPlayerHealthHUDTest::RunTest(const FString& Parameters)
{
    TGuardValue<uint64> RestoreFrame(GFrameCounter, GFrameCounter);
    FPlayerHealthWorld Fixture;
    auto* Player = Fixture.SpawnPlayer();
    if (!TestNotNull(TEXT("Player"), Player)) return false;
    auto* AI = Fixture.SpawnEnemyController();
    auto* Class = LoadClass<UCandyFightWeaponHUD>(nullptr,
        TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightWeaponHUD.BP_CandyFightWeaponHUD_C"));
    if (!TestNotNull(TEXT("HUD Blueprint"), Class)) return false;
    auto* HUD = NewObject<UCandyFightWeaponHUD>(Fixture.World, Class);
    HUD->Initialize(); HUD->TakeWidget();
    if (!TestNotNull(TEXT("Designer bar binds"), HUD->HealthBar.Get())) return false;
    const FLinearColor Base(.3f,.04f,.15f,.8f);
    HUD->HealthBar->SetFillColorAndOpacity(Base);
    auto* Slot = Cast<UCanvasPanelSlot>(HUD->GetWidgetFromName(TEXT("HealthPanel"))->Slot);
    const auto Position = Slot->GetPosition();
    Player->TakeDamage(20, FDamageEvent(), AI, nullptr);
    HUD->Connect(Player->Combat); HUD->Connect(Player->Combat);
    TestEqual(TEXT("Connect reads real health"), HUD->HealthText->GetText().ToString(), FString(TEXT("80 / 100")));
    TestFalse(TEXT("Connect does not flash"), HUD->bHealthFlashActive);
    TestEqual(TEXT("Unique damage binding"), Player->OnPlayerDamaged.GetAllObjects().Num(), 1);
    Player->TakeDamage(10, FDamageEvent(), AI, nullptr);
    TestEqual(TEXT("HUD event updates percent"), HUD->HealthBar->GetPercent(), .7f);
    TestEqual(TEXT("Flash begins"), HUD->HealthBar->GetFillColorAndOpacity(), HUD->DamageFlashColor);
    const auto Advance = [&](int32 Frames)
    {
        for (int32 I=0; I<Frames; ++I) { ++GFrameCounter; Fixture.World->Tick(LEVELTICK_All, .05f); HUD->UpdateHealthFlash(); }
    };
    Advance(2);
    Player->TakeDamage(10, FDamageEvent(), AI, nullptr);
    Advance(2);
    TestTrue(TEXT("Repeated hit restarts duration"), HUD->bHealthFlashActive);
    Fixture.World->GetWorldSettings()->SetPauserPlayerState(Fixture.World->SpawnActor<APlayerState>());
    Advance(6);
    TestTrue(TEXT("Pause freezes flash despite UI updates"), HUD->bHealthFlashActive);
    Fixture.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    Advance(2);
    TestEqual(TEXT("Original color restored after repeated hits"), HUD->HealthBar->GetFillColorAndOpacity(), Base);
    Player->Heal(10);
    TestFalse(TEXT("Healing does not flash"), HUD->bHealthFlashActive);
    HUD->SetHealthDisplay(1, 2);
    TestEqual(TEXT("Display API does not change player"), Player->Health, 70.f);
    HUD->Connect(Player->Combat);
    TestEqual(TEXT("Reconnect corrects display"), HUD->HealthBar->GetPercent(), .7f);
    Player->TakeDamage(10, FDamageEvent(), AI, nullptr);
    HUD->Connect(nullptr);
    TestEqual(TEXT("Disconnect restores authored color"), HUD->HealthBar->GetFillColorAndOpacity(), Base);
    TestEqual(TEXT("Damage unbound"), Player->OnPlayerDamaged.GetAllObjects().Num(), 0);
    Player->TakeDamage(10, FDamageEvent(), AI, nullptr);
    TestEqual(TEXT("Disconnected HUD not updated"), HUD->HealthBar->GetPercent(), .6f);
    HUD->Connect(Player->Combat); HUD->DamageFlashDuration = 0;
    Player->TakeDamage(10, FDamageEvent(), AI, nullptr);
    TestFalse(TEXT("Zero duration disables feedback"), HUD->bHealthFlashActive);
    TestEqual(TEXT("Layout preserved"), Slot->GetPosition(), Position);
    HUD->NativeDestruct();
    TestEqual(TEXT("Destruction unbinds"), Player->OnPlayerDamaged.GetAllObjects().Num(), 0);
    HUD->ReleaseSlateResources(true);
    auto* Rebuilt = NewObject<UCandyFightWeaponHUD>(Fixture.World, Class);
    Rebuilt->Initialize(); Rebuilt->Connect(Player->Combat); Rebuilt->TakeWidget(); Rebuilt->NativeConstruct();
    TestEqual(TEXT("Rebuilt HUD reads current health"), Rebuilt->HealthText->GetText().ToString(), FString(TEXT("40 / 100")));
    TestFalse(TEXT("Rebuilt HUD has no stale flash"), Rebuilt->bHealthFlashActive);
    Rebuilt->NativeDestruct(); Rebuilt->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPlayerHealthProjectileTest, "CandyFight.PlayerHealth.ProjectilesAndCombatAtZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightPlayerHealthProjectileTest::RunTest(const FString& Parameters)
{
    FPlayerHealthWorld Fixture;
    auto* Player = Fixture.SpawnPlayer();
    if (!TestNotNull(TEXT("Player"), Player)) return false;
    auto* AI = Fixture.SpawnEnemyController();
    auto* Config = NewObject<UCandyFightWeaponConfig>(); Config->Damage = 10.f;
    Player->GetCapsuleComponent()->SetCapsuleSize(42.f, 88.f);
    for (int32 Pellets : {1,3,8})
    {
        Player->Heal(100);
        auto* Shot = Fixture.World->SpawnActor<ACandyFightProjectileShot>();
        TArray<FVector> Directions; Directions.Init(FVector::ForwardVector, Pellets);
        Shot->Initialize(*Config, FVector(0,0,88), Directions, AI->GetPawn(), AI, nullptr, nullptr, 0.);
        Shot->AdvanceTo(.3);
        TestEqual(TEXT("Enemy projectile pellet sum"), Player->Health, 100.f - 10.f*Pellets);
    }
    Player->Heal(100);
    auto* Shot = Fixture.World->SpawnActor<ACandyFightProjectileShot>();
    // The second pellet reaches the same capsule on a later frame.
    TArray<FVector> Directions = {FVector::ForwardVector, FVector(1,.03,0).GetSafeNormal()};
    Shot->Initialize(*Config, FVector(0,0,88), Directions, AI->GetPawn(), AI, nullptr, nullptr, 0.);
    Shot->AdvanceTo(.1935);
    TestEqual(TEXT("First pellet arrives alone"), Player->Health, 90.f);
    Shot->AdvanceTo(.3);
    TestEqual(TEXT("Later pellet uses delta, not shot total"), Player->Health, 80.f);
    auto* Combat = Player->Combat.Get();
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    Combat->WeaponConfigs = {Config}; Combat->WeaponStates.SetNum(1); Combat->WeaponStates[0].Initialize(*Config, 0.);
    TestTrue(TEXT("Equip player weapon"), Combat->EquipTestWeapon(1));
    Player->TakeDamage(999, FDamageEvent(), AI, nullptr);
    const int32 Ammo = Combat->GetAmmo();
    Player->AimWorldLocation = FVector(2000,0,108);
    Player->bHasValidAim = true;
    Combat->FireShot(*Player, 0.);
    TestEqual(TEXT("Can shoot at zero health"), Combat->GetAmmo(), Ammo-1);
    TestTrue(TEXT("Can begin reload at zero health"), Combat->WeaponStates[0].BeginReload(*Config, 1.));
    auto* Enemy = CastChecked<ACandyFightEnemyCharacter>(AI->GetPawn());
    Enemy->EnemyConfig = NewObject<UCandyFightEnemyConfig>();
    Enemy->SetActorLocation(FVector(500,0,88)); Enemy->SetActorRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Enemy still sees zero-health player"), Enemy->Perception->CanSee(Player));
    return true;
}
#endif
