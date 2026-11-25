#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/CandyFightCharacter.h"
#include "Engine/World.h"
#include "Sound/SoundWave.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "Components/BoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAudioRulesTest, "CandyFight.Audio.ReloadAndDryRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAudioRulesTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->FireMode = ECandyFightFireMode::Automatic;
    Config->ShotsPerSecond = 10.f;
    FCandyFightWeaponState State;
    State.Initialize(*Config, 0.);
    State.Ammo = 1;
    State.CommitShot(*Config, 0., true);
    TestTrue(TEXT("Held last automatic round enters dry state"), State.bDryFiring);
    TestFalse(TEXT("Held empty automatic does not reload"), State.bReloading);
    State.Advance(*Config, 10.);
    TestEqual(TEXT("Dry idle does not refill"), State.Ammo, 0);
    TestEqual(TEXT("Dry idle does not fire shots"), State.ShotsFired, 1);
    State.ReleaseTrigger(*Config, 10.);
    TestTrue(TEXT("Release starts reload"), State.bReloading && !State.bDryFiring);
    TestFalse(TEXT("Repeated R does not reset progress"), State.BeginReload(*Config, 10.5));
    State.Advance(*Config, 11.21);
    TestEqual(TEXT("Whole magazine refilled"), State.Ammo, Config->MagazineCapacity);
    State.Ammo = 1;
    State.CommitShot(*Config, 12., true);
    TestTrue(TEXT("R can end held dry state"), State.BeginReload(*Config, 12.1));
    TestFalse(TEXT("R stops dry loop"), State.bDryFiring);
    State.CancelReload(); State.Ammo = 1;
    State.CommitShot(*Config, 14., false);
    TestTrue(TEXT("Released last shot reloads immediately"), State.bReloading && !State.bDryFiring);

    Config->FireMode = ECandyFightFireMode::Shotgun;
    Config->MagazineCapacity = 3; Config->ReloadSeconds = .5f;
    State.Initialize(*Config, 0.);
    TestTrue(TEXT("Spawn full shotgun ready"), State.CanFire(*Config, 0.));
    State.Ammo = 1; State.CommitShot(*Config, 0., true);
    TestFalse(TEXT("Shotgun never dry loops"), State.bDryFiring);
    State.Advance(*Config, .4); State.Advance(*Config, .9);
    State.Advance(*Config, 1.4); State.Advance(*Config, 1.9);
    TestFalse(TEXT("Full shotgun has no reload or rack lock"), State.bReloading || State.bNeedsRack);
    FCandyFightFireIntent Intent; Intent.Press(State.CanFire(*Config, 1.9));
    TestTrue(TEXT("Click at full boundary can fire"), Intent.Consume(Config->FireMode));
    TestFalse(TEXT("Holding still cannot repeat shotgun fire"), Intent.Consume(Config->FireMode));
    State.CommitShot(*Config, 2.);
    State.BeginReload(*Config, 3.);
    State.CommitShot(*Config, 3.1);
    TestFalse(TEXT("Preparation can be interrupted with ammo"), State.bReloading);
    State.Advance(*Config, 10.);
    TestEqual(TEXT("No delayed insertion after preparation interruption"), State.Ammo, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAudioScheduleTest, "CandyFight.Audio.GameTimeSchedule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAudioScheduleTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->FireMode = ECandyFightFireMode::Shotgun;
    Config->ReloadSeconds = .5f;
    FCandyFightWeaponState State; State.Initialize(*Config, 0.); State.Ammo = 0; State.BeginReload(*Config, 0.);
    FCandyFightAudioSchedule Schedule;
    double Offset = 0.;
    TestFalse(TEXT("Preparation has no audio even when duration is supplied"), Schedule.ConsumeAction(State, .3, .277641723, Offset));
    State.Advance(*Config, .4);
    TestFalse(TEXT("L03 waits for end alignment"), Schedule.ConsumeAction(State, .62, .277641723, Offset));
    TestTrue(TEXT("L03 plays near 0.622 seconds"), Schedule.ConsumeAction(State, .623, .277641723, Offset));
    TestTrue(TEXT("L03 uses elapsed offset"), FMath::IsNearlyEqual(Offset, .000641723, .00001));
    TestFalse(TEXT("No duplicate shell audio"), Schedule.ConsumeAction(State, .8, .277641723, Offset));
    State.Advance(*Config, .9);
    TestTrue(TEXT("Next shell audio independent"), Schedule.ConsumeAction(State, 1.123, .277641723, Offset));
    State.CancelReload();
    TestFalse(TEXT("Cancelled insertion cannot schedule"), Schedule.ConsumeAction(State, 5., .277641723, Offset));
    State.BeginReload(*Config, 5.); State.Advance(*Config, 5.4);
    TestFalse(TEXT("Entirely missed clip not backfilled"), Schedule.ConsumeAction(State, 7., .277641723, Offset));
    Schedule.Reset();
    TestFalse(TEXT("Missing audio does not schedule"), Schedule.ConsumeAction(State, 5.7, 0., Offset));
    State.CancelReload(); Config->ReloadSeconds = .1f; Config->ShellReloadStartDelay = 0.f;
    State.BeginReload(*Config, 8.); Schedule.Reset();
    TestTrue(TEXT("Short cycle starts sound immediately"), Schedule.ConsumeAction(State, 8., .277641723, Offset));
    TestEqual(TEXT("Short cycle does not seek or time stretch"), Offset, 0.);
    State.CancelReload();
    TestFalse(TEXT("Cancelled short cycle cannot replay"), Schedule.ConsumeAction(State, 8.05, .277641723, Offset));

    Config->FireMode = ECandyFightFireMode::SemiAutomatic;
    Config->ReloadSeconds = 1.2f;
    State.Initialize(*Config, 0.); State.Ammo = 0; State.BeginReload(*Config, 0.); Schedule.Reset();
    TestTrue(TEXT("R01 begins at magazine cycle start"), Schedule.ConsumeAction(State, .0001, 1.2, Offset));
    State.Advance(*Config, .5);
    for (int32 I = 0; I < 120; ++I) State.Advance(*Config, .5);
    TestEqual(TEXT("Frozen game time cannot complete reload"), State.Ammo, 0);
    State.Advance(*Config, 1.21);
    TestEqual(TEXT("Reload completes without an audio device"), State.Ammo, Config->MagazineCapacity);

    Config->ReloadSeconds = 1.8f;
    State.Initialize(*Config, 0.); State.Ammo = 0; State.BeginReload(*Config, 0.); Schedule.Reset();
    TestFalse(TEXT("M01 waits for end alignment"), Schedule.ConsumeAction(State, .24, 1.555737, Offset));
    TestTrue(TEXT("M01 begins near 0.244 seconds"), Schedule.ConsumeAction(State, .245, 1.555737, Offset));

    Config->FireMode = ECandyFightFireMode::Automatic;
    Config->ShotsPerSecond = 10.;
    for (int32 FPS : {30, 60, 120})
    {
        State.Initialize(*Config, 0.); State.Ammo = 1; State.CommitShot(*Config, 0., true); Schedule.Reset();
        int32 Requests = 0; double Last = -100.;
        for (int32 I = 0; I < FPS * 5; ++I)
        {
            const double Now = double(I) / FPS;
            if (Schedule.ConsumeDry(State, Now, .857324))
            {
                TestTrue(TEXT("Dry clips never overlap"), Now - Last >= .8572);
                TestTrue(TEXT("First dry sound waits for next shot interval"), Now >= .0999);
                Last = Now; ++Requests;
            }
        }
        TestEqual(FString::Printf(TEXT("%d FPS dry repeat count"), FPS), Requests, 6);
        TestTrue(TEXT("One sound on returning from hitch"), Schedule.ConsumeDry(State, 50., .857324));
        TestFalse(TEXT("No catch-up burst after hitch"), Schedule.ConsumeDry(State, 50., .857324));
        TestFalse(TEXT("Frozen game time cannot repeat audio"), Schedule.ConsumeDry(State, 50., .857324));
        State.ReleaseTrigger(*Config, 50.);
        TestFalse(TEXT("Release cancels next dry sound"), Schedule.ConsumeDry(State, 60., .857324));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAudioIntegrationTest, "CandyFight.Audio.CombatRequests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAudioIntegrationTest::RunTest(const FString& Parameters)
{
    // Headless transient world. No editor PIE or listening/visual acceptance.
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Character = World->SpawnActor<ACandyFightCharacter>();
    auto* Combat = Character->Combat.Get();
    auto* Config = NewObject<UCandyFightWeaponConfig>(Combat);
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    Config->FireMode = ECandyFightFireMode::Shotgun; Config->PelletCount = 12;
    Config->Damage = 8.f; Config->BaseSpread = Config->MaxSpread = 8.f;
    Combat->WeaponConfigs.Add(Config); Combat->WeaponStates.SetNum(1);
    Combat->WeaponStates[0].Initialize(*Config, 0.);
    TestTrue(TEXT("Test equip"), Combat->EquipTestWeapon(1));
    Character->AimWorldLocation = FVector(2000., 0., 20.);
    Character->bHasValidAim = true;
    auto* Target = World->SpawnActor<ACandyFightTarget>(FVector(1000., 0., 0.), FRotator::ZeroRotator);
    Target->HitBox->SetBoxExtent(FVector(40., 400., 200.));
    Combat->FireShot(*Character, 0.);
    TestEqual(TEXT("Twelve pellet shot requests only one fire sound"), Combat->FireSoundRequests, 1);
    TestEqual(TEXT("Twelve horizontal rays"), Combat->LastShotEnds.Num(), 12);
    for (const auto& End : Combat->LastShotEnds)
        TestTrue(TEXT("Every pellet ray stays horizontal"), FMath::IsNearlyEqual(End.Z, Combat->LastShotStart.Z, .001));
    TestEqual(TEXT("Launch never damages distant targets instantly"), Target->Health, 200.f);
    TestEqual(TEXT("One flight instance owns twelve pellets"), Combat->ActiveShots.Last()->GetPellets().Num(), 12);
    Combat->ActiveShots.Last()->AdvanceTo(.25);
    TestEqual(TEXT("Twelve hits aggregate into 96 damage"), Target->LastDamage, 96.f);
    TestEqual(TEXT("One damage event per target"), Target->HitCount, 1);
    TestEqual(TEXT("Target loses exactly 96 HP"), Target->Health, 104.f);
    Target->Destroy();
    TestEqual(TEXT("One ammunition consumed"), Combat->GetAmmo(), Config->MagazineCapacity - 1);
    TestFalse(TEXT("Invalid equip rejected"), Combat->EquipTestWeapon(99));
    const int32 Serial = Combat->WeaponStates[0].PhaseSerial;
    TestTrue(TEXT("Same equip accepted without reset"), Combat->EquipTestWeapon(1));
    TestEqual(TEXT("Same/invalid equip preserve state"), Combat->WeaponStates[0].PhaseSerial, Serial);
    Config->FireMode = ECandyFightFireMode::Automatic; Config->PelletCount = 1; Config->ShotsPerSecond = 10.;
    Config->MagazineCapacity = 1000;
    for (int32 FPS : {30, 60, 120})
    {
        auto& State = Combat->WeaponStates[0]; State.Initialize(*Config, 0.);
        const int32 Before = Combat->FireSoundRequests;
        for (int32 I = 0; I < FPS * 2; ++I)
        {
            const double Now = double(I) / FPS;
            if (State.CanFire(*Config, Now)) Combat->FireShot(*Character, Now);
        }
        TestEqual(TEXT("Sound requests match committed shots at all frame rates"), Combat->FireSoundRequests - Before, State.ShotsFired);
        TestTrue(TEXT("Automatic request cadence"), FMath::Abs(State.ShotsFired - 20) <= 1);
    }
    auto& State = Combat->WeaponStates[0]; State.Ammo = 1;
    Combat->FireIntent.Press(true); Combat->FireShot(*Character, 5.);
    TestTrue(TEXT("Actual combat held last shot enters dry state"), Combat->IsDryFiring());
    Combat->ReleaseFireIntent();
    TestTrue(TEXT("Focus/control release starts reload"), Combat->IsReloading() && !Combat->IsDryFiring());
    TestFalse(TEXT("Focus/control release clears buffered input"), Combat->FireIntent.Consume(Config->FireMode));
    TestTrue(TEXT("Reload requires fresh trigger"), Combat->bFireNeedsRelease);
    State.Advance(*Config, State.ReloadEndTime + .01);
    TestFalse(TEXT("Completed reload does not recreate held fire"), Combat->FireIntent.Consume(Config->FireMode));
    TestTrue(TEXT("Reload completion keeps release gate"), Combat->bFireNeedsRelease);
    Combat->StopWeaponAudio();
    TestTrue(TEXT("Audio cleanup clears all owned handles"), Combat->ShotAudio.IsEmpty() && Combat->FadingActionAudio.IsEmpty() && !Combat->ActionAudio.IsValid() && !Combat->DryAudio.IsValid());
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAudioAssetsTest, "CandyFight.Audio.ImportedAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightAudioAssetsTest::RunTest(const FString& Parameters)
{
    const TCHAR* Names[] = { TEXT("SemiFire"), TEXT("AutoFire"), TEXT("ShotgunFire"), TEXT("DryFire"), TEXT("SemiReload"), TEXT("AutoReload"), TEXT("ShellReload"), TEXT("Rack"), TEXT("ShellReload_L03") };
    TArray<USoundWave*> Sounds;
    for (const TCHAR* Name : Names)
    {
        const FString Path = FString(TEXT("/Game/CandyFight/Audio/Weapons/S_Weapon_")) + Name;
        auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
        Sounds.Add(Sound);
        if (TestNotNull(Path, Sound))
        {
            TestTrue(TEXT("Finite positive duration"), FMath::IsFinite(Sound->GetDuration()) && Sound->GetDuration() > 0.f);
            TestFalse(TEXT("One-shot asset is not looped"), Sound->bLooping);
            TestEqual(TEXT("Mono import"), Sound->NumChannels, 1);
        }
    }
    if (Sounds[4]) TestTrue(TEXT("R01 is 1.2 seconds"), FMath::IsNearlyEqual(Sounds[4]->GetDuration(), 1.2f, .001f));
    const TCHAR* Configs[] = { TEXT("SemiAuto"), TEXT("Automatic"), TEXT("Shotgun") };
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Config = LoadObject<UCandyFightWeaponConfig>(nullptr, *(FString(TEXT("/Game/CandyFight/Data/Weapons/DA_Weapon_")) + Configs[I]));
        if (!TestNotNull(TEXT("Config loads"), Config)) continue;
        TestTrue(TEXT("Audio migration persisted"), Config->AudioMigrationVersion >= 1);
        TestNotNull(TEXT("Hard fire reference cooks with config"), Config->FireSound.Get());
        if (I == 2)
        {
            TestNotNull(TEXT("Hard shell reference"), Config->ShellSound.Get());
            TestNull(TEXT("Default rack reference removed"), Config->RackSound.Get());
            TestTrue(TEXT("Shotgun migration persisted"), Config->AudioMigrationVersion >= 2);
            TestEqual(TEXT("Selected L03 bound"), Config->ShellSound.Get(), static_cast<USoundBase*>(Sounds[8]));
        }
        else TestNotNull(TEXT("Hard reload reference"), Config->ReloadSound.Get());
        if (I == 1) TestNotNull(TEXT("Hard dry reference"), Config->DryFireSound.Get());
    }
    return true;
}
#endif
