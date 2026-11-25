#include "CandyFightEnemyHealthTestObserver.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "Engine/DamageEvents.h"

void UCandyFightEnemyHealthTestObserver::Defeated(ACandyFightEnemyCharacter* Enemy, AController* InstigatorController)
{
    ++DefeatCount;
    ReentrantDamage = Enemy->TakeDamage(500.f, FDamageEvent(), InstigatorController, nullptr);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/AI/CandyFightEnemyConfig.h"
#include "CandyFight/AI/CandyFightEnemyAIController.h"
#include "CandyFight/AI/CandyFightEnemyWeaponComponent.h"
#include "CandyFight/UI/CandyFightEnemyHealthWidget.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include <limits>

namespace
{
struct FHealthWorld
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FHealthWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    ~FHealthWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEnemyHealthTest, "CandyFight.EnemyHealth.DamageAndDefeat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightEnemyHealthTest::RunTest(const FString& Parameters)
{
    FHealthWorld Fixture;
    auto* World = Fixture.World;
    auto* Player = World->SpawnActor<APlayerController>();
    auto* OtherAI = World->SpawnActor<AAIController>();
    auto* Enemy = World->SpawnActor<ACandyFightEnemyCharacter>();
    auto* Other = World->SpawnActor<ACandyFightEnemyCharacter>(FVector(1000,0,0), FRotator::ZeroRotator);
    auto* Config = NewObject<UCandyFightEnemyConfig>();
    Config->MaxHealth = 150.f;
    Enemy->EnemyConfig = Config;
    Enemy->DispatchBeginPlay();
    TestEqual(TEXT("Configured initial health"), Enemy->Health, 150.f);
    TestEqual(TEXT("Friendly fire ignored"), Enemy->TakeDamage(20, FDamageEvent(), OtherAI, nullptr), 0.f);
    TestEqual(TEXT("Unattributed damage ignored"), Enemy->TakeDamage(20, FDamageEvent(), nullptr, nullptr), 0.f);
    TestEqual(TEXT("Negative ignored"), Enemy->TakeDamage(-20, FDamageEvent(), Player, nullptr), 0.f);
    TestEqual(TEXT("Nonfinite ignored"), Enemy->TakeDamage(std::numeric_limits<float>::infinity(), FDamageEvent(), Player, nullptr), 0.f);
    auto* Display = NewObject<UCandyFightEnemyHealthWidget>();
    Display->Initialize(); Display->TakeWidget();
    Enemy->OnHealthChanged.AddDynamic(Display, &UCandyFightEnemyHealthWidget::SetHealthDisplay);
    TestEqual(TEXT("Player damage accepted"), Enemy->TakeDamage(30, FDamageEvent(), Player, nullptr), 30.f);
    TestEqual(TEXT("Health changes"), Enemy->Health, 120.f);
    TestEqual(TEXT("Health event updates widget"), Display->HealthText->GetText().ToString(), FString(TEXT("120 / 150")));
    TestEqual(TEXT("Health event updates bar"), Display->HealthBar->GetPercent(), .8f);
    TestEqual(TEXT("Other enemy independent"), Other->Health, 100.f);
    auto* Observer = NewObject<UCandyFightEnemyHealthTestObserver>();
    Enemy->OnEnemyDefeated.AddDynamic(Observer, &UCandyFightEnemyHealthTestObserver::Defeated);
    auto* Gun = World->SpawnActor<ACandyFightWeapon>();
    Enemy->Combat->EquippedWeapon = Gun;
    TestEqual(TEXT("Overkill returns actual loss"), Enemy->TakeDamage(999, FDamageEvent(), Player, nullptr), 120.f);
    TestEqual(TEXT("Health clamps to zero"), Enemy->Health, 0.f);
    TestTrue(TEXT("Enemy defeated and removed"), Enemy->bDefeated && Enemy->IsActorBeingDestroyed());
    TestFalse(TEXT("No collision remains"), Enemy->GetActorEnableCollision());
    TestFalse(TEXT("No bar remains"), Enemy->HealthWidget->IsVisible());
    TestFalse(TEXT("Cannot shoot when defeated"), Enemy->IsCombatShotAllowed());
    TestTrue(TEXT("Equipped weapon removed"), Gun->IsActorBeingDestroyed());
    TestEqual(TEXT("Exactly one defeat notification"), Observer->DefeatCount, 1);
    TestEqual(TEXT("Reentrant damage ignored"), Observer->ReentrantDamage, 0.f);
    TestEqual(TEXT("Later hit ignored"), Enemy->TakeDamage(50, FDamageEvent(), Player, nullptr), 0.f);
    TestEqual(TEXT("No duplicate defeat"), Observer->DefeatCount, 1);
    Display->ReleaseSlateResources(true);
    Other->EnemyConfig = NewObject<UCandyFightEnemyConfig>();
    Other->EnemyConfig->MaxHealth = std::numeric_limits<float>::quiet_NaN();
    Other->DispatchBeginPlay();
    TestEqual(TEXT("Invalid maximum independently falls back"), Other->Health, 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEnemyHealthAssetTest, "CandyFight.EnemyHealth.AssetAndOcclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightEnemyHealthAssetTest::RunTest(const FString& Parameters)
{
    auto* Class = LoadClass<ACandyFightEnemyCharacter>(nullptr, TEXT("/Game/CandyFight/Blueprints/AI/BP_CandyFightEnemy.BP_CandyFightEnemy_C"));
    if (!TestNotNull(TEXT("Enemy asset"), Class)) return false;
    auto* CDO = Class->GetDefaultObject<ACandyFightEnemyCharacter>();
    const auto WidgetClass = CDO->HealthWidget->GetWidgetClass();
    if (!TestNotNull(TEXT("Health widget configured"), WidgetClass.Get())) return false;
    TestEqual(TEXT("Designer widget assigned"), WidgetClass->GetPathName(), FString(TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightEnemyHealthBar.BP_CandyFightEnemyHealthBar_C")));
    TestEqual(TEXT("Screen space"), CDO->HealthWidget->GetWidgetSpace(), EWidgetSpace::Screen);
    TestEqual(TEXT("No UI collision"), CDO->HealthWidget->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    auto* Display = NewObject<UCandyFightEnemyHealthWidget>(GetTransientPackage(), WidgetClass);
    Display->Initialize();
    TestNotNull(TEXT("Designer health bar binds"), Display->HealthBar.Get());
    TestNotNull(TEXT("Designer text binds"), Display->HealthText.Get());
    FHealthWorld Fixture;
    auto* Enemy = Fixture.World->SpawnActor<ACandyFightEnemyCharacter>(FVector(1000,0,0), FRotator::ZeroRotator);
    const FVector Camera(0,0,118);
    TestTrue(TEXT("Clear view shows bar"), Enemy->IsHealthBarUnoccluded(Camera, nullptr));
    auto* Wall = Fixture.World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Wall->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(25,100,200)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
    Wall->SetActorLocation(FVector(500,0,118));
    TestFalse(TEXT("Wall occludes bar"), Enemy->IsHealthBarUnoccluded(Camera, nullptr));
    TestTrue(TEXT("Viewer is ignored"), Enemy->IsHealthBarUnoccluded(Camera, Wall));
    Wall->SetActorLocation(FVector(500,500,118));
    TestTrue(TEXT("Bar recovers after obstacle moves"), Enemy->IsHealthBarUnoccluded(Camera, nullptr));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEnemyProjectileDefeatTest, "CandyFight.EnemyHealth.ProjectileAndAICleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightEnemyProjectileDefeatTest::RunTest(const FString& Parameters)
{
    FHealthWorld Fixture;
    auto* World = Fixture.World;
    World->CreateAISystem();
    auto* Player = World->SpawnActor<APlayerController>();
    auto* Shooter = World->SpawnActor<AActor>();
    auto* EnemyClass = LoadClass<ACandyFightEnemyCharacter>(nullptr, TEXT("/Game/CandyFight/Blueprints/AI/BP_CandyFightEnemy.BP_CandyFightEnemy_C"));
    if (!TestNotNull(TEXT("Enemy Blueprint"), EnemyClass)) return false;
    for (int32 Pellets : {1, 3, 8})
    {
        auto* Enemy = World->SpawnActor<ACandyFightEnemyCharacter>(EnemyClass, FVector(1000,0,88), FRotator::ZeroRotator);
        Enemy->DispatchBeginPlay();
        auto* AI = World->SpawnActor<ACandyFightEnemyAIController>(); AI->Possess(Enemy);
        auto* Gun = Enemy->Combat->EquippedWeapon.Get();
        TestNotNull(TEXT("Enemy equipped at runtime"), Gun);
        auto* Config = NewObject<UCandyFightWeaponConfig>(); Config->Damage = 100.f;
        auto* Shot = World->SpawnActor<ACandyFightProjectileShot>();
        TArray<FVector> Directions; Directions.Init(FVector::ForwardVector, Pellets);
        Shot->Initialize(*Config, FVector(0,0,88), Directions, Shooter, Player, nullptr, nullptr, 0.);
        Shot->AdvanceTo(.3);
        TestTrue(TEXT("Player projectile kills enemy"), Enemy->bDefeated);
        TestTrue(TEXT("AI controller destroyed"), AI->IsActorBeingDestroyed());
        TestNull(TEXT("Controller detached"), AI->GetPawn());
        TestTrue(TEXT("Weapon destroyed"), !IsValid(Gun));
        TestFalse(TEXT("Combat no longer ticks"), Enemy->Combat->IsComponentTickEnabled());
        TestFalse(TEXT("Driver no longer ticks"), Enemy->WeaponDriver->IsComponentTickEnabled());
    }
    return true;
}
#endif
