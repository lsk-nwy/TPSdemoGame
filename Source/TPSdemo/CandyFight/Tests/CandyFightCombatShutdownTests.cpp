#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/GarbageCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightCombatShutdownTest,
    "CandyFight.Weapons.ShutdownWithoutWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightCombatShutdownTest::RunTest(const FString& Parameters)
{
    auto Prepare = [](UCandyFightCombatComponent* Combat)
    {
        auto* Config = NewObject<UCandyFightWeaponConfig>(Combat);
        Config->WeaponClass = ACandyFightWeapon::StaticClass();
        Config->FireMode = ECandyFightFireMode::Automatic;
        Combat->WeaponConfigs.Add(Config);
        Combat->WeaponStates.SetNum(1);
        Combat->WeaponStates[0].Initialize(*Config, 0.);
        Combat->EquippedIndex = 0;
        Combat->WeaponStates[0].Ammo = 1;
        Combat->WeaponStates[0].CommitShot(*Config, 0., true);
        Combat->FireIntent.Press(true);
    };

    // 精确覆盖崩溃前提：配置和状态还在，但组件已经没有 UWorld。
    // 分别走直接销毁、Shutdown 后重复销毁、GC 的 BeginDestroy 回调。
    for (int32 Route = 0; Route < 3; ++Route)
    {
        auto* Combat = NewObject<UCandyFightCombatComponent>();
        Combat->OnComponentCreated(); // Ensure GC routes through OnComponentDestroyed.
        Prepare(Combat);
        TestNull(TEXT("Teardown fixture has no world"), Combat->GetWorld());
        const int32 OriginalPhase = Combat->WeaponStates[0].PhaseSerial;
        if (Route == 2)
        {
            TWeakObjectPtr<UCandyFightCombatComponent> WeakCombat = Combat;
            Combat = nullptr;
            CollectGarbage(RF_NoFlags, true);
            TestFalse(TEXT("Worldless equipped component collected safely"), WeakCombat.IsValid());
            continue;
        }
        if (Route == 0) Combat->DestroyComponent();
        else { Combat->StopFire(); Combat->ShutdownCombat(); Combat->DestroyComponent(); }
        Combat->ShutdownCombat();
        Combat->StopFire();
        TestFalse(TEXT("Shutdown cannot start reload"), Combat->IsReloading());
        TestFalse(TEXT("Shutdown ends dry fire"), Combat->IsDryFiring());
        TestEqual(TEXT("No transient reload phase during destruction"), Combat->WeaponStates[0].PhaseSerial, OriginalPhase);
        TestEqual(TEXT("Shutdown cannot refill ammunition"), Combat->GetAmmo(), 0);
        TestFalse(TEXT("No held or queued fire survives"), Combat->FireIntent.Consume(ECandyFightFireMode::Automatic));
        TestEqual(TEXT("No action audio scheduled"), Combat->ActionSoundRequests, 0);
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Pawn = World->SpawnActor<APawn>();
    auto* Combat = NewObject<UCandyFightCombatComponent>(Pawn);
    Combat->RegisterComponent();
    Prepare(Combat);
    Combat->BeginPlay();
    Combat->WeaponStates[0].Ammo = 1;
    Combat->WeaponStates[0].CommitShot(*Combat->GetCurrentConfig(), 0., true);
    // 正常游戏中的松键必须继续触发空弹装填，不能为修复退出而破坏规则。
    Combat->StopFire();
    TestTrue(TEXT("Live trigger release still starts automatic reload"), Combat->IsReloading());
    Combat->EquippedWeapon = World->SpawnActor<ACandyFightWeapon>();
    TWeakObjectPtr<ACandyFightWeapon> Gun = Combat->EquippedWeapon.Get();
    auto* Shot = World->SpawnActor<ACandyFightProjectileShot>();
    TWeakObjectPtr<ACandyFightProjectileShot> FlyingShot = Shot;
    Combat->ActiveShots.Add(Shot);
    World->BeginTearingDown();
    Combat->EndPlay(EEndPlayReason::EndPlayInEditor);
    TestTrue(TEXT("Gun destroyed at EndPlay"), !Gun.IsValid() || Gun->IsActorBeingDestroyed());
    TestTrue(TEXT("In-flight shot destroyed at EndPlay"), !FlyingShot.IsValid() || FlyingShot->IsActorBeingDestroyed());
    TestTrue(TEXT("Projectile tracking cleared"), Combat->ActiveShots.IsEmpty());
    TestFalse(TEXT("Reload cancelled at EndPlay"), Combat->IsReloading());
    TestFalse(TEXT("Focus callback removed"), Combat->ActivationHandle.IsValid());
    TestFalse(TEXT("Component cannot tick after shutdown"), Combat->IsComponentTickEnabled());
    Combat->UnregisterComponent();
    Combat->DestroyComponent();
    Combat->ShutdownCombat();
    World->DestroyWorld(false);
    CollectGarbage(RF_NoFlags, true);
    return true;
}
#endif
