#if WITH_DEV_AUTOMATION_TESTS
#include "CandyFight/CandyFightCrosshairSnap.h"
#include "CandyFight/CandyFightAimFraming.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightCrosshairSnapResponseTest, "CandyFight.Aiming.Snap.Response",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightCrosshairSnapResponseTest::RunTest(const FString& Parameters)
{
    const FIntRect Rect(0,0,1920,1080);
    const FVector2D Start(500,500), Target(700,500);
    for (int32 Steps : {1, 4, 12, 24})
    {
        FCandyFightCrosshairSnapState State;
        for (int32 Frame = 0; Frame < Steps; ++Frame)
        {
            State.UpdateInput(Start, FVector2D::ZeroVector, Rect, Frame);
            State.SnapTo(Target, .12f, .12f / Steps, Frame);
            const FVector2D Once = State.DisplayPosition;
            State.SnapTo(Target + FVector2D(50,0), .12f, .12f / Steps, Frame);
            TestEqual(TEXT("Repeated camera evaluation does not advance or retarget interpolation"), State.DisplayPosition, Once);
        }
        TestTrue(TEXT("95 percent response is independent of frame count"), State.DisplayPosition.Equals(FVector2D(690,500), .0001));
        TestEqual(TEXT("Attraction never changes original mouse input"), State.RawPosition, Start);
        State.Release();
        TestTrue(TEXT("Release preserves visible position"), State.DisplayPosition.Equals(FVector2D(690,500), .0001));
        TestEqual(TEXT("Release preserves mouse input"), State.RawPosition, Start);
        State.UpdateInput(State.RawPosition, FVector2D(20,-10), Rect, Steps);
        TestTrue(TEXT("Mouse movement applies before release catch-up"), State.DisplayPosition.Equals(FVector2D(710,490), .0001));
        State.ReturnToInput(.12f, Steps);
        TestTrue(TEXT("Release uses the same 95 percent response"), State.DisplayPosition.Equals(FVector2D(529.5,490), .0001));
    }
    FCandyFightCrosshairSnapState State;
    State.UpdateInput(Start, FVector2D::ZeroVector, Rect, 1);
    State.SnapTo(Target, 0.f, 0.f, 1);
    TestEqual(TEXT("Zero time gives instant snap"), State.DisplayPosition, Target);
    State.UpdateInput(Start, FVector2D(10,5), Rect, 2);
    TestEqual(TEXT("Mouse movement applies before attraction"), State.DisplayPosition, Target + FVector2D(10,5));
    State.UpdateInput(State.RawPosition, FVector2D(10,5), Rect, 2);
    TestEqual(TEXT("Repeated input frame consumes delta only once"), State.DisplayPosition, Target + FVector2D(10,5));
    State.SnapTo(Target, .12f, 0.f, 2);
    TestEqual(TEXT("Paused frame does not interpolate"), State.DisplayPosition, Target + FVector2D(10,5));
    State.SnapTo(Target, .12f, -1.f, 3);
    TestEqual(TEXT("Negative elapsed time does not interpolate"), State.DisplayPosition, Target + FVector2D(10,5));
    State.SnapTo(Target, std::numeric_limits<float>::quiet_NaN(), .1f, 4);
    TestFalse(TEXT("Invalid parameter cannot poison position"), State.DisplayPosition.ContainsNaN());
    State.Reset();
    TestFalse(TEXT("Reset clears capture"), State.bSnapping);
    TestFalse(TEXT("Reset clears initialization"), State.bInitialized);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightCrosshairSnapReleaseTest, "CandyFight.Aiming.Snap.ReleaseContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightCrosshairSnapReleaseTest::RunTest(const FString& Parameters)
{
    const FIntRect Rect(0,0,1920,1080);
    for (int32 Steps : {4, 12, 24})
    {
        FCandyFightCrosshairSnapState State;
        State.UpdateInput(FVector2D(500,500), FVector2D::ZeroVector, Rect, 0);
        State.SnapTo(FVector2D(600,500), .12f, 1.f, 0);
        // Drag past the target until raw input leads the displayed cursor.
        for (int32 Frame = 1; Frame <= 10; ++Frame)
        {
            State.UpdateInput(State.RawPosition, FVector2D(20,0), Rect, Frame);
            State.SnapTo(FVector2D(600,500), .12f, 1.f/60, Frame);
        }
        const FVector2D Raw = State.RawPosition, Display = State.DisplayPosition;
        TestTrue(TEXT("Fixture dragged input ahead of attraction"), Raw.X > Display.X);
        State.Release();
        State.ReturnToInput(.12f, 10);
        TestEqual(TEXT("Release and repeated frame preserve the visible position"), State.DisplayPosition, Display);
        TestEqual(TEXT("Release retains all accumulated drag input"), State.RawPosition, Raw);
        for (int32 I = 1; I <= Steps; ++I)
        {
            const FVector2D Before = State.DisplayPosition;
            State.UpdateInput(State.RawPosition, FVector2D(60./Steps,0), Rect, 10+I);
            State.ReturnToInput(.12f/Steps, 10+I);
            TestTrue(TEXT("Continued drag never rebounds toward the old target"), State.DisplayPosition.X > Before.X);
            TestTrue(TEXT("Catch-up cannot overshoot input"), State.DisplayPosition.X <= State.RawPosition.X);
            const FVector2D Once = State.DisplayPosition;
            State.Release();
            State.ReturnToInput(.12f, 10+I);
            TestEqual(TEXT("Repeated release/evaluation does not restart or advance return"), State.DisplayPosition, Once);
        }
        TestTrue(TEXT("Catch-up is frame-rate independent while mouse moves"),
            State.DisplayPosition.Equals(Raw + FVector2D(60,0) + (Display-Raw)*.05, .0001));
        const FIntRect Resized(100,50,1380,770);
        const FVector2D NormalizedDisplay = CandyFightAimFraming::Normalize(State.DisplayPosition, Rect);
        State.UpdateInput(State.RawPosition, FVector2D::ZeroVector, Resized, 100);
        TestTrue(TEXT("Viewport change preserves release progress"),
            CandyFightAimFraming::Normalize(State.DisplayPosition, Resized).Equals(NormalizedDisplay, 1.e-9));
        State.UpdateInput(State.RawPosition, FVector2D::ZeroVector, Resized, 101);
        State.SnapTo(FVector2D(600,400), .12f, .01f, 101);
        TestTrue(TEXT("Reacquisition cancels catch-up"), State.bSnapping && !State.bReturning);
        const FVector2D Snapped = State.DisplayPosition;
        State.ReturnToInput(1.f, 102);
        TestEqual(TEXT("Return cannot overwrite reacquired attraction"), State.DisplayPosition, Snapped);
    }
    FCandyFightCrosshairSnapState Instant;
    Instant.UpdateInput(FVector2D(500,500), FVector2D::ZeroVector, Rect, 1);
    Instant.SnapTo(FVector2D(600,500), 0.f, .01f, 1);
    Instant.Release();
    TestEqual(TEXT("Zero snap time immediately restores mouse position"), Instant.DisplayPosition, Instant.RawPosition);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightCrosshairSnapViewportTest, "CandyFight.Aiming.Snap.ViewportAndMotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightCrosshairSnapViewportTest::RunTest(const FString& Parameters)
{
    using namespace CandyFightAimFraming;
    const FIntRect Rect(100,50,1380,770), Resized(20,40,1940,1120);
    FCandyFightCrosshairSnapState State;
    const FVector2D Raw = ToPixels(FVector2D(.25,.75), Rect);
    const FVector2D Anchor = ToPixels(FVector2D(.6,.4), Rect);
    State.UpdateInput(Raw, FVector2D::ZeroVector, Rect, 1);
    State.SnapTo(Anchor, 0.f, .1f, 1);
    State.UpdateInput(Raw, FVector2D::ZeroVector, Resized, 2);
    TestTrue(TEXT("Resize preserves original normalized position"), Normalize(State.RawPosition, Resized).Equals(FVector2D(.25,.75), 1.e-9));
    TestTrue(TEXT("Resize preserves displayed normalized position independently"), Normalize(State.DisplayPosition, Resized).Equals(FVector2D(.6,.4), 1.e-9));
    const FVector2D MovingAnchor = ToPixels(FVector2D(.65,.45), Resized);
    State.SnapTo(MovingAnchor, .12f, .12f, 2);
    TestTrue(TEXT("Moving anchor can be followed without mouse input"), (State.DisplayPosition - MovingAnchor).Size() < 6.);
    State.Release();
    State.UpdateInput(State.RawPosition, FVector2D(99999,99999), Resized, 3);
    TestEqual(TEXT("Viewport edge clamps both positions"), State.DisplayPosition, FVector2D(1939,1119));
    State.UpdateInput(State.RawPosition, FVector2D(-1,-1), Resized, 4);
    TestEqual(TEXT("Excess edge input does not delay movement back"), State.DisplayPosition, FVector2D(1938,1118));
    return true;
}
#endif
