#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightShotgunPatternTest, "CandyFight.Weapons.ThreeSectorPattern", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightShotgunPatternTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightWeaponConfig>();
    Config->WeaponClass = ACandyFightWeapon::StaticClass();
    Config->FireMode = ECandyFightFireMode::Shotgun;
    Config->PelletCount = 12;
    FString Error;
    TestTrue(TEXT("Default three-sector config is valid"), Config->ValidateConfiguration(Error));
    FRandomStream Random(724), Replay(724), Different(725);
    TArray<float> Angles, Replayed, Other;
    CandyFightWeapons::SampleAngles(*Config, 8.f, Random, Angles);
    CandyFightWeapons::SampleAngles(*Config, 8.f, Replay, Replayed);
    CandyFightWeapons::SampleAngles(*Config, 8.f, Different, Other);
    TestTrue(TEXT("Seed reproduces complete shot"), Angles == Replayed);
    TestFalse(TEXT("Different seed changes pattern"), Angles == Other);
    const auto FirstShot = Angles;
    CandyFightWeapons::SampleAngles(*Config, 8.f, Random, Angles);
    TestFalse(TEXT("Successive shots change directions"), FirstShot == Angles);

    // Distribution checks use angular observations, not the sampler's output ordering.
    int32 Bins[3][10] = {};
    int32 ClosePairShots = 0, AsymmetricShots = 0;
    for (int32 Shot = 0; Shot < 10000; ++Shot)
    {
        CandyFightWeapons::SampleAngles(*Config, 8.f, Random, Angles);
        if (!TestEqual(TEXT("Every shot has twelve pellets"), Angles.Num(), 12)) return false;
        int32 Counts[3] = {};
        for (float Angle : Angles)
        {
            if (!TestTrue(TEXT("Finite angle inside fan"), FMath::IsFinite(Angle) && Angle >= -8.f && Angle <= 8.f)) return false;
            const int32 Sector = Angle < -4.f ? 0 : Angle < 4.f ? 1 : 2;
            const float MinAngle = Sector == 0 ? -8.f : Sector == 1 ? -4.f : 4.f;
            const float Width = Sector == 1 ? 8.f : 4.f;
            ++Counts[Sector];
            ++Bins[Sector][FMath::Clamp(int32((Angle - MinAngle) / Width * 10), 0, 9)];
        }
        if (!TestTrue(TEXT("Each shot has exact 2/8/2 population"), Counts[0] == 2 && Counts[1] == 8 && Counts[2] == 2)) return false;
        Angles.Sort();
        bool bClose = false, bAsymmetric = false;
        for (int32 I = 1; I < Angles.Num(); ++I) bClose |= Angles[I] - Angles[I-1] < .01f;
        for (int32 I = 0; I < 6; ++I) bAsymmetric |= FMath::Abs(Angles[I] + Angles[11-I]) > .1f;
        ClosePairShots += bClose;
        AsymmetricShots += bAsymmetric;
    }
    for (int32 Sector = 0; Sector < 3; ++Sector)
        for (int32 Bin = 0; Bin < 10; ++Bin)
        {
            const int32 Expected = Sector == 1 ? 8000 : 2000;
            TestTrue(TEXT("Each sector uniformly covers its angular range"), FMath::Abs(Bins[Sector][Bin] - Expected) < Expected * .1);
        }
    TestTrue(TEXT("Sampling permits closely clustered pellets"), ClosePairShots > 100);
    TestTrue(TEXT("Sampling does not mirror left and right"), AsymmetricShots > 9900);

    Config->PelletCount = 7; Config->ShotgunSidePelletCount = 1;
    for (float Ratio : {.25f, .75f})
    {
        Config->ShotgunCenterWidthRatio = Ratio;
        TestTrue(TEXT("Custom pattern validates"), Config->ValidateConfiguration(Error));
        for (float Spread : {3.f, 12.f})
        {
            CandyFightWeapons::SampleAngles(*Config, Spread, Random, Angles);
            int32 Counts[3] = {};
            for (float Angle : Angles)
            {
                TestTrue(TEXT("Custom angle stays inside scaled fan"), FMath::Abs(Angle) <= Spread);
                ++Counts[Angle < -Spread * Ratio ? 0 : Angle < Spread * Ratio ? 1 : 2];
            }
            TestTrue(TEXT("Custom population is 1/5/1"), Counts[0] == 1 && Counts[1] == 5 && Counts[2] == 1);
        }
    }
    CandyFightWeapons::SampleAngles(*Config, 0.f, Random, Angles);
    TestEqual(TEXT("Zero spread retains all pellets"), Angles.Num(), 7);
    for (float Angle : Angles) TestEqual(TEXT("Zero spread points forward"), Angle, 0.f);
    Config->PelletCount = 1; Config->ShotgunSidePelletCount = 0;
    TestTrue(TEXT("Single center pellet supported"), Config->ValidateConfiguration(Error));
    CandyFightWeapons::SampleAngles(*Config, 8.f, Random, Angles);
    TestEqual(TEXT("Zero side counts produce no side pellets"), Angles.Num(), 1);
    Config->PelletCount = 64; Config->ShotgunSidePelletCount = 31;
    TestTrue(TEXT("Maximum pellet count supported"), Config->ValidateConfiguration(Error));
    Config->ShotgunSidePelletCount = 32;
    TestFalse(TEXT("Must leave at least one center pellet"), Config->ValidateConfiguration(Error));
    Config->ShotgunSidePelletCount = -1;
    TestFalse(TEXT("Negative side count rejected"), Config->ValidateConfiguration(Error));
    Config->ShotgunSidePelletCount = MAX_int32;
    TestFalse(TEXT("Huge side count rejected without overflow"), Config->ValidateConfiguration(Error));
    Config->ShotgunSidePelletCount = 2;
    for (float Ratio : {0.f, 1.f, -.1f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        Config->ShotgunCenterWidthRatio = Ratio;
        TestFalse(TEXT("Invalid central width rejected"), Config->ValidateConfiguration(Error));
    }
    Config->PelletCount = 1;
    for (auto Mode : {ECandyFightFireMode::SemiAutomatic, ECandyFightFireMode::Automatic})
    {
        Config->FireMode = Mode;
        TestTrue(TEXT("Other guns ignore shotgun-only fields"), Config->ValidateConfiguration(Error));
        FRandomStream Actual(99), Expected(99);
        CandyFightWeapons::SampleAngles(*Config, 3.f, Actual, Angles);
        TestEqual(TEXT("Other guns still emit one direction"), Angles.Num(), 1);
        TestEqual(TEXT("Other guns keep existing uniform sampling"), Angles[0], float(Expected.FRandRange(-3.f, 3.f)));
    }
    return true;
}
#endif
