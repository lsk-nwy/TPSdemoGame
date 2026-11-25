#if WITH_DEV_AUTOMATION_TESTS

#include "CandyFight/CandyFightCameraConfig.h"
#include "CandyFight/CandyFightCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAimingTest, "CandyFight.Aiming.Configuration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightAimingTest::RunTest(const FString& Parameters)
{
    FString Error;
    UCandyFightCameraConfig* Config = NewObject<UCandyFightCameraConfig>();
    TestTrue(TEXT("Default aiming configuration is valid"), Config->ValidateConfiguration(Error));
    TestEqual(TEXT("Default release camera blend lasts 0.20 seconds"), Config->AimAssistReleaseBlendSeconds, .20f);
    auto* ExistingCamera = LoadObject<UCandyFightCameraConfig>(nullptr,
        TEXT("/Game/CandyFight/Data/Camera/DA_CandyFightCamera_Default.DA_CandyFightCamera_Default"));
    if (TestNotNull(TEXT("Existing camera asset loads without migration"), ExistingCamera))
        TestEqual(TEXT("Existing camera loads the new release blend default"), ExistingCamera->AimAssistReleaseBlendSeconds, .20f);
    for (float Invalid : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        Config->AimAssistReleaseBlendSeconds = Invalid;
        TestFalse(TEXT("Release camera blend requires finite nonnegative duration"), Config->ValidateConfiguration(Error));
    }
    Config->AimAssistReleaseBlendSeconds = 0.f;
    TestTrue(TEXT("Zero supports the previous instant camera response"), Config->ValidateConfiguration(Error));
    Config->AimAssistReleaseBlendSeconds = .20f;
    Config->bEnableAimScreenOffset = false;
    Config->AimScreenEdgeMarginX = 0.0f;
    Config->AimScreenEdgeMarginY = 0.5f;
    Config->AimOffsetInterpSpeed = 0.0f;
    TestTrue(TEXT("Zero values support disabled offset and instant response"), Config->ValidateConfiguration(Error));
    Config->AimScreenEdgeMarginX = 0.51f;
    TestFalse(TEXT("Margins greater than half the screen are rejected"), Config->ValidateConfiguration(Error));
    Config->AimScreenEdgeMarginX = 0.1f;
    Config->AimScreenEdgeMarginY = -0.01f;
    TestFalse(TEXT("Negative margins are rejected"), Config->ValidateConfiguration(Error));
    Config->AimScreenEdgeMarginY = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite margins are rejected"), Config->ValidateConfiguration(Error));
    Config->AimScreenEdgeMarginY = 0.1f;
    Config->AimOffsetInterpSpeed = -1.0f;
    TestFalse(TEXT("Negative interpolation speed is rejected"), Config->ValidateConfiguration(Error));
    Config->AimOffsetInterpSpeed = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite interpolation speed is rejected"), Config->ValidateConfiguration(Error));
    Config->AimOffsetInterpSpeed = 0.0f;
    Config->AimOffsetStrength = std::numeric_limits<float>::quiet_NaN();
    Config->AimMaxOffsetDistance = -1.0f;
    TestTrue(TEXT("Deprecated world-offset settings do not invalidate screen framing"), Config->ValidateConfiguration(Error));

    const ACandyFightCharacter* Character = GetDefault<ACandyFightCharacter>();
    TestFalse(TEXT("Movement cannot override aiming yaw"), Character->GetCharacterMovement()->bOrientRotationToMovement);
    TestFalse(TEXT("Controller smoothing cannot override aiming yaw"), Character->GetCharacterMovement()->bUseControllerDesiredRotation);
    TestNotNull(TEXT("An editable weapon hold point is available"), Character->WeaponHoldPoint.Get());
    return true;
}

#endif
