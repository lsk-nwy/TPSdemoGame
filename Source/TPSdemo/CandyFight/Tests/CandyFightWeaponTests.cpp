#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Components/BoxComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightWeaponRulesTest, "CandyFight.Weapons.StateAndInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightWeaponRulesTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    FString Error;
    TestTrue(TEXT("Default definition valid"), Config->ValidateConfiguration(Error));
    FCandyFightWeaponState State;
    State.Initialize(*Config, 0.);
    TestEqual(TEXT("Starts full"), State.Ammo, 12);
    FCandyFightFireIntent Trigger;
    Trigger.Press(State.CanFire(*Config, 0.));
    TestTrue(TEXT("First semi-auto press fires"), Trigger.Consume(Config->FireMode));
    State.CommitShot(*Config, 0.);
    TestFalse(TEXT("Holding semi-auto does not repeat"), Trigger.Consume(Config->FireMode));
    Trigger.Release(); Trigger.Press(State.CanFire(*Config, .1));
    TestFalse(TEXT("Fast click in cooldown discarded"), Trigger.Consume(Config->FireMode));
    TestFalse(TEXT("Rejected click is not buffered"), Trigger.Consume(Config->FireMode));
    TestFalse(TEXT("No overspeed"), State.CanFire(*Config, .19));
    TestTrue(TEXT("Next shot after interval"), State.CanFire(*Config, .2));
    Trigger.Reset(); Trigger.Press(true); Trigger.Release();
    TestTrue(TEXT("Down and up in same frame retains one click"), Trigger.Consume(Config->FireMode));
    TestFalse(TEXT("Same-frame click is consumed once"), Trigger.Consume(Config->FireMode));
    Trigger.Press(true);
    TestTrue(TEXT("Automatic holds"), Trigger.Consume(ECandyFightFireMode::Automatic));
    TestTrue(TEXT("Automatic repeats while held"), Trigger.Consume(ECandyFightFireMode::Automatic));
    Trigger.Release(); TestFalse(TEXT("Automatic stops on release"), Trigger.Consume(ECandyFightFireMode::Automatic));
    Trigger.Press(true); Trigger.Reset(); TestFalse(TEXT("Console/focus reset clears trigger"), Trigger.Consume(ECandyFightFireMode::Automatic));
    TestTrue(TEXT("Reload starts with partial magazine"), State.BeginReload(*Config, .2));
    TestFalse(TEXT("Repeated reload ignored"), State.BeginReload(*Config, .3));
    TestFalse(TEXT("Reload prevents fire"), State.CanFire(*Config, .5));
    State.CancelReload(); State.Advance(*Config, 2.);
    TestEqual(TEXT("Cancelled reload gives no ammunition"), State.Ammo, 11);
    TestTrue(TEXT("Reload restarts"), State.BeginReload(*Config, 2.));
    TestFalse(TEXT("No early refill"), State.Advance(*Config, 3.));
    TestTrue(TEXT("Refill at completion"), State.Advance(*Config, 3.21));
    TestEqual(TEXT("Full after reload"), State.Ammo, 12);
    TestFalse(TEXT("Full magazine reload ignored"), State.BeginReload(*Config, 4.));
    State.Ammo = 1; State.CommitShot(*Config, 5.);
    TestFalse(TEXT("Empty cannot fire"), State.CanFire(*Config, 5.));
    TestTrue(TEXT("Last round automatically starts reload"), State.bReloading);
    TestEqual(TEXT("Automatic reload starts at shot time"), State.ReloadStartTime, 5.);
    State.Advance(*Config, 6.21);
    TestEqual(TEXT("Automatic reload fills magazine"), State.Ammo, 12);
    State.Initialize(*Config, 0.); State.CommitShot(*Config, 0.);
    State.Advance(*Config, .1); TestEqual(TEXT("Recovery delay respected"), State.Spread, .9f);
    State.Advance(*Config, .2); TestTrue(TEXT("Only elapsed recovery after delay used"), FMath::IsNearlyEqual(State.Spread, .65f, .0001f));
    State.Advance(*Config, 1.); TestEqual(TEXT("Recovery stops at base"), State.Spread, Config->BaseSpread);
    State.Ammo = 100;
    for (int I=0; I<20; ++I) State.CommitShot(*Config, 2.);
    TestEqual(TEXT("Bloom clamped"), State.Spread, Config->MaxSpread);
    // Stored states are not reinitialized when another definition is equipped.
    FCandyFightWeaponState Other; Other.Initialize(*Config, 0.);
    TestEqual(TEXT("Other magazine independent"), Other.Ammo, 12);
    TestFalse(TEXT("Original cooldown persists"), State.CanFire(*Config, 2.05));
    Config->ShotsPerSecond = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite rate rejected"), Config->ValidateConfiguration(Error));
    Config->ShotsPerSecond = 5.f; Config->BaseSpread = 5.f;
    TestFalse(TEXT("Inverted spread rejected"), Config->ValidateConfiguration(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightShellReloadTest, "CandyFight.Weapons.ShellReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightShellReloadTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->FireMode = ECandyFightFireMode::Shotgun;
    Config->PelletCount = 12;
    Config->MagazineCapacity = 8;
    Config->ReloadSeconds = .5f;
    Config->ShotsPerSecond = 1.25f;
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    FString Error;
    Config->RackSeconds = -1.f; Config->RackVolume = -1.f;
    TestTrue(TEXT("Inactive rack values do not invalidate config"), Config->ValidateConfiguration(Error));
    Config->ShellReloadStartDelay = -1.f;
    TestFalse(TEXT("Negative preparation rejected"), Config->ValidateConfiguration(Error));
    Config->ShellReloadStartDelay = .4f;
    FCandyFightWeaponState State;
    State.Initialize(*Config, 0.); State.Ammo = 1; State.CommitShot(*Config, 0.);
    TestTrue(TEXT("Empty shotgun starts preparation"), State.ReloadPhase == ECandyFightReloadPhase::ShellStart);
    TestFalse(TEXT("Empty cannot interrupt"), State.CanFire(*Config, .3));
    TestFalse(TEXT("Repeated R does not restart preparation"), State.BeginReload(*Config, .2));
    State.Advance(*Config, .4);
    TestEqual(TEXT("Preparation adds no ammo"), State.Ammo, 0);
    TestTrue(TEXT("Preparation enters insertion"), State.ReloadPhase == ECandyFightReloadPhase::Shell);
    TestFalse(TEXT("No early first shell"), State.Advance(*Config, .899));
    State.Advance(*Config, .9);
    TestEqual(TEXT("First shell at 0.9 seconds"), State.Ammo, 1);
    TestTrue(TEXT("Loaded shell can fire"), State.CanFire(*Config, .9));
    State.CommitShot(*Config, .9);
    TestTrue(TEXT("Last-shell interruption restarts preparation"), State.ReloadPhase == ECandyFightReloadPhase::ShellStart);
    State.Advance(*Config, 1.3); State.Advance(*Config, 1.8);
    TestEqual(TEXT("Restart has full preparation plus insertion"), State.Ammo, 1);
    State.Advance(*Config, 2.3);
    TestEqual(TEXT("Next shell needs only half a second"), State.Ammo, 2);
    State.CommitShot(*Config, 2.3);
    TestFalse(TEXT("Partial reload interrupted by real shot"), State.bReloading);
    State.Advance(*Config, 10.);
    TestEqual(TEXT("Cancelled insertion cannot add ammo"), State.Ammo, 1);
    State.BeginReload(*Config, 10.);
    TestTrue(TEXT("Preparation with ammo can be interrupted"), State.CanFire(*Config, 10.1));
    State.CommitShot(*Config, 10.1);
    TestTrue(TEXT("Empty after preparation interruption starts again"), State.ReloadPhase == ECandyFightReloadPhase::ShellStart);
    State.CancelReload(); State.Ammo = 2; State.ResumeEquipped(*Config, 20.);
    TestFalse(TEXT("Reequip partial shotgun does not resume reload"), State.bReloading);
    State.Ammo = 0; State.ResumeEquipped(*Config, 20.);
    TestTrue(TEXT("Reequip empty shotgun starts preparation"), State.ReloadPhase == ECandyFightReloadPhase::ShellStart);
    State.Advance(*Config, 20.4);
    State.CancelReload(); State.Advance(*Config, 30.);
    TestEqual(TEXT("Switch cancels uncompleted insertion"), State.Ammo, 0);

    for (int32 FPS : {30, 60, 120})
    {
        State.Initialize(*Config, 0.); State.Ammo = 0; State.BeginReload(*Config, 0.);
        FCandyFightAudioSchedule Schedule;
        int32 Requests = 0; double Offset = 0.;
        for (int32 Frame = 0; Frame <= FPS * 4.5; ++Frame)
        {
            const double Now = double(Frame) / FPS;
            State.Advance(*Config, Now);
            if (Schedule.ConsumeAction(State, Now, .277641723, Offset)) ++Requests;
            if (Now < .89999) TestEqual(TEXT("No shell before first deadline"), State.Ammo, 0);
            if (Now >= 4.4) TestEqual(TEXT("Full within frame tolerance at 4.4 seconds"), State.Ammo, 8);
        }
        TestEqual(TEXT("One L03 request per inserted shell at each FPS"), Requests, 8);
        TestFalse(TEXT("Full immediately ready without rack"), State.bReloading || State.bNeedsRack);
        TestTrue(TEXT("Full shotgun can fire immediately"), State.CanFire(*Config, 4.5));
        TestFalse(TEXT("Full R ignored"), State.BeginReload(*Config, 4.5));
        State.CancelReload(); State.ResumeEquipped(*Config, 5.);
        TestTrue(TEXT("Switch back to full shotgun has no rack wait"), State.CanFire(*Config, 5.));
    }
    Config->ShellReloadStartDelay = 0.f;
    State.Initialize(*Config, 0.); State.Ammo = 0; State.BeginReload(*Config, 0.);
    TestTrue(TEXT("Zero preparation skips directly to insertion"), State.ReloadPhase == ECandyFightReloadPhase::Shell);
    State.Advance(*Config, .5); TestEqual(TEXT("Zero delay first shell at insertion deadline"), State.Ammo, 1);
    Config->ShellReloadStartDelay = .7f; Config->ReloadSeconds = .3f;
    State.Initialize(*Config, 0.); State.Ammo = 2; State.BeginReload(*Config, 0.);
    State.NextShotTime = 2.;
    FCandyFightFireIntent Intent; Intent.Press(State.CanFire(*Config, .1));
    TestFalse(TEXT("Cooldown click not buffered"), Intent.Consume(Config->FireMode));
    State.Advance(*Config, .7); State.Advance(*Config, 1.);
    TestEqual(TEXT("Custom timing respected"), State.Ammo, 3);
    TestFalse(TEXT("Rejected click remains discarded"), Intent.Consume(Config->FireMode));
    State.Advance(*Config, 50.);
    TestEqual(TEXT("Hitch adds at most one shell"), State.Ammo, 4);
    State.Advance(*Config, 50.);
    TestEqual(TEXT("No shell backlog after hitch"), State.Ammo, 4);
    State.Initialize(*Config, 0.); State.Ammo = 0; State.BeginReload(*Config, 0.);
    for (int32 I = 0; I < 120; ++I) State.Advance(*Config, .2);
    TestEqual(TEXT("Paused preparation cannot add ammo"), State.Ammo, 0);
    TestTrue(TEXT("Paused preparation stays in same phase"), State.ReloadPhase == ECandyFightReloadPhase::ShellStart);
    Config->FireMode = ECandyFightFireMode::Automatic; Config->ReloadSeconds = 2.f;
    State.Initialize(*Config, 0.); State.Ammo = 1; State.CommitShot(*Config, 0.);
    TestTrue(TEXT("Automatic retains magazine reload"), State.ReloadPhase == ECandyFightReloadPhase::Magazine);
    State.Advance(*Config, 2.);
    TestEqual(TEXT("Automatic fills whole magazine"), State.Ammo, 8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightWeaponRateTest, "CandyFight.Weapons.FrameRatesAndPellets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightWeaponRateTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->ShotsPerSecond = 10.f; Config->MagazineCapacity = 1000;
    Config->FireMode = ECandyFightFireMode::Automatic;
    for (int32 FPS : {30, 60, 120})
    {
        FCandyFightWeaponState State; State.Initialize(*Config, 0.);
        for (int32 Frame = 0; Frame < FPS * 5; ++Frame)
        {
            const double Now = double(Frame) / FPS;
            if (State.CanFire(*Config, Now)) State.CommitShot(*Config, Now);
        }
        TestTrue(FString::Printf(TEXT("%d FPS: 50 shots within one shot"), FPS), FMath::Abs(State.ShotsFired - 50) <= 1);
        const int32 Before = State.ShotsFired;
        if (State.CanFire(*Config, 20.)) State.CommitShot(*Config, 20.);
        TestEqual(TEXT("Long hitch causes only one shot"), State.ShotsFired - Before, 1);
        TestFalse(TEXT("No catch-up shot in same frame"), State.CanFire(*Config, 20.));
    }
    {
        FCandyFightWeaponState State; State.Initialize(*Config, 0.);
        double Now = 0.;
        for (int32 Frame = 0; Now < 5.; ++Frame)
        {
            if (State.CanFire(*Config, Now)) State.CommitShot(*Config, Now);
            Now += Frame % 2 ? .0107 : .0088;
        }
        TestTrue(TEXT("Irregular rendered frame times do not accumulate automatic cadence drift"), FMath::Abs(State.ShotsFired - 50) <= 1);
    }
    Config->FireMode = ECandyFightFireMode::Shotgun;
    Config->PelletCount = 12;
    FRandomStream Random(42);
    for (int32 Shot=0; Shot<100; ++Shot)
    {
        TArray<float> Angles;
        CandyFightWeapons::SampleAngles(*Config, 8.f, Random, Angles);
        TestEqual(TEXT("Twelve pellets"), Angles.Num(), 12);
        int32 Left = 0, Center = 0, Right = 0;
        for (float Angle : Angles)
        {
            TestTrue(TEXT("Pellet stays inside fan"), Angle >= -8.f && Angle <= 8.f);
            if (Angle < -4.f) ++Left;
            else if (Angle < 4.f) ++Center;
            else ++Right;
        }
        TestEqual(TEXT("Left sector count"), Left, 2);
        TestEqual(TEXT("Center sector count"), Center, 8);
        TestEqual(TEXT("Right sector count"), Right, 2);
    }
    FCandyFightWeaponState State; State.Initialize(*Config, 0.); State.CommitShot(*Config, 0.);
    TestEqual(TEXT("Shotgun consumes one shell"), State.Ammo, Config->MagazineCapacity-1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightWeaponCollisionTest, "CandyFight.Weapons.CollisionAndDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightWeaponCollisionTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ACandyFightTarget* Near = World->SpawnActor<ACandyFightTarget>(FVector(300.,0.,90.), FRotator::ZeroRotator);
    ACandyFightTarget* Far = World->SpawnActor<ACandyFightTarget>(FVector(600.,0.,90.), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Near target spawned"), Near) || !TestNotNull(TEXT("Far target spawned"), Far)) { World->DestroyWorld(false); return false; }
    FCollisionQueryParams Params;
    FHitResult Hit; FVector End;
    UCandyFightCombatComponent::TracePellet(World, FVector(0.,0.,90.), FVector::ForwardVector, 1000.f, Params, Hit, End);
    TestTrue(TEXT("First blocker is near target"), Hit.GetActor() == Near);
    TestTrue(TEXT("Trace stops at front surface"), FMath::IsNearlyEqual(End.X, 260., .1));
    UCandyFightCombatComponent::TracePellet(World, FVector(0.,0.,90.), FVector::ForwardVector, 100.f, Params, Hit, End);
    TestFalse(TEXT("No damage beyond range"), Hit.bBlockingHit);
    TestTrue(TEXT("Miss extends to range"), End.Equals(FVector(100.,0.,90.), .1));
    FHitResult WallHit;
    TestTrue(TEXT("Body-to-muzzle sweep detects an intervening blocker"), World->SweepSingleByChannel(WallHit,
        FVector(0.,0.,90.), FVector(400.,0.,90.), FQuat::Identity, CandyFightWeapons::TraceChannel, FCollisionShape::MakeSphere(1.f), Params));
    TestTrue(TEXT("Muzzle overlap detects inside geometry"), World->OverlapBlockingTestByChannel(FVector(300.,0.,90.), FQuat::Identity,
        CandyFightWeapons::TraceChannel, FCollisionShape::MakeSphere(1.f), Params));
    Near->TakeDamage(64.f, FDamageEvent(), nullptr, nullptr);
    TestEqual(TEXT("Grouped shotgun damage"), Near->Health, 136.f);
    TestEqual(TEXT("Single damage event for group"), Near->HitCount, 1);
    Near->TakeDamage(200.f, FDamageEvent(), nullptr, nullptr);
    TestEqual(TEXT("Health clamped to zero"), Near->Health, 0.f);
    TestFalse(TEXT("Dead target stops blocking"), Near->GetActorEnableCollision());
    UCandyFightCombatComponent::TracePellet(World, FVector(0.,0.,90.), FVector::ForwardVector, 1000.f, Params, Hit, End);
    TestTrue(TEXT("Next target reachable after near target dies"), Hit.GetActor() == Far);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightWeaponAssetsTest, "CandyFight.Weapons.BlueprintAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightWeaponAssetsTest::RunTest(const FString& Parameters)
{
    UClass* CharacterClass = LoadClass<ACandyFightCharacter>(nullptr, TEXT("/Game/CandyFight/Blueprints/BP_CandyFightCharacter.BP_CandyFightCharacter_C"));
    if (!TestNotNull(TEXT("Configured player blueprint"), CharacterClass)) return false;
    const auto* Character = CharacterClass->GetDefaultObject<ACandyFightCharacter>();
    const auto* Combat = Character->Combat.Get();
    if (!TestNotNull(TEXT("Combat component"), Combat)) return false;
    TestEqual(TEXT("Three test definitions"), Combat->WeaponConfigs.Num(), 3);
    for (const auto& Config : Combat->WeaponConfigs)
    {
        if (!TestNotNull(TEXT("Weapon definition assigned"), Config.Get())) continue;
        FString Error;
        if (!TestTrue(TEXT("Definition validates: ") + Config->GetName(), Config->ValidateConfiguration(Error))) continue;
        const auto* Weapon = Config->WeaponClass->GetDefaultObject<ACandyFightWeapon>();
        TestNotNull(TEXT("Editable visual root"), Weapon->WeaponVisualRoot.Get());
        TestNotNull(TEXT("Editable body"), Weapon->Body.Get());
        TestNotNull(TEXT("Independent muzzle"), Weapon->Muzzle.Get());
        TestTrue(TEXT("Muzzle independent of visual scale"), Weapon->Muzzle->GetAttachParent() == Weapon->MountRoot);
    }
    TestNotNull(TEXT("Fire input action"), Combat->FireAction.Get());
    TestNotNull(TEXT("Reload input action"), Combat->ReloadAction.Get());
    TestNotNull(TEXT("HUD blueprint"), Combat->HUDClass.Get());
    if (TestNotNull(TEXT("Independent combat mappings"), Combat->CombatMappingContext.Get()))
    {
        const auto& Mappings = Combat->CombatMappingContext->GetMappings();
        TestEqual(TEXT("Only fire and reload mappings"), Mappings.Num(), 2);
        for (const auto& Mapping : Mappings)
            TestTrue(TEXT("No normal switch key bindings"), Mapping.Key == EKeys::LeftMouseButton || Mapping.Key == EKeys::R);
    }
    return true;
}
#endif
