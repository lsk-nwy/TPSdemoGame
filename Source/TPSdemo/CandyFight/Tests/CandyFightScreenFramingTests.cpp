#if WITH_DEV_AUTOMATION_TESTS

#include "CandyFight/CandyFightAimFraming.h"
#include "CandyFight/CandyFightCameraConfig.h"
#include "CandyFight/UI/CandyFightCrosshairWidget.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"
#include "Misc/AutomationTest.h"

namespace
{
    FSceneViewProjectionData MakeProjection(const FIntRect& Rect, const FVector& Center, double Yaw = 0., double Pitch = -55.)
    {
        FMinimalViewInfo View;
        View.Rotation = FRotator(Pitch, Yaw, 0.);
        View.Location = Center - View.Rotation.Vector() * 2000.;
        View.FOV = 90.f;
        View.bConstrainAspectRatio = false;
        FSceneViewProjectionData Data;
        Data.SetViewRectangle(Rect);
        FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View, AspectRatio_MaintainXFOV, Rect, Data);
        Data.ViewOrigin = View.Location;
        Data.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
            FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));
        return Data;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightScreenFramingProjectionTest, "CandyFight.Aiming.ScreenFramingProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightScreenFramingProjectionTest::RunTest(const FString& Parameters)
{
    using namespace CandyFightAimFraming;
    const FVector2D Margins(0.1, 0.1);
    double MaxError = 0.;
    // Includes letterbox/editor viewport origins and DPI-scaled viewport dimensions.
    for (const FIntRect& Rect : {FIntRect(0,0,1920,1080), FIntRect(0,0,1280,720),
        FIntRect(75,125,2635,1205), FIntRect(100,90,1124,858), FIntRect(0,0,960,540)})
    for (double Yaw : {0., 73., 180., -90.})
    for (const FVector& Center : {FVector(0,0,100), FVector(1200,-350,190)})
    for (double X : {0., 0.25, 0.5, 0.75, 1.})
    for (double Y : {0., 0.25, 0.5, 0.75, 1.})
    {
        const FVector2D Cursor = ToPixels(FVector2D(X,Y), Rect);
        const FVector2D Desired = Target(Cursor, Rect, Margins);
        auto Data = MakeProjection(Rect, Center, Yaw);
        FVector Offset;
        if (!TestTrue(TEXT("Framing can be solved"), SolveOffset(Data, Desired, Center, Offset))) continue;
        TestEqual(TEXT("Framing never changes camera height"), Offset.Z, 0.);
        Data.ViewOrigin += Offset;
        FVector2D Actual;
        TestTrue(TEXT("Actor is in front of final camera"), FSceneView::ProjectWorldToScreen(Center, Rect, Data.ComputeViewProjectionMatrix(), Actual));
        const double Error = (Actual-ToPixels(Desired,Rect)).Size();
        MaxError = FMath::Max(MaxError, Error);
        TestTrue(TEXT("Actor projection matches target within one physical pixel"), Error <= 1.);

        // Actual muzzle-height aiming must still reproject to the same on-screen cursor.
        FVector Aim;
        TestTrue(TEXT("Final camera intersects muzzle-height aim plane"), IntersectPlane(Data, Cursor, Center.Z+20., Aim));
        FVector2D AimPixel;
        TestTrue(TEXT("Aim point projects in front of camera"), FSceneView::ProjectWorldToScreen(Aim, Rect, Data.ComputeViewProjectionMatrix(), AimPixel));
        TestTrue(TEXT("Final aim remains on cursor"), (AimPixel-Cursor).Size() < 1.);
        TestTrue(TEXT("Aim is horizontal at muzzle height"), FMath::IsNearlyEqual(Aim.Z, Center.Z+20., 0.001));

        // Translate player and base camera together: no lag, drift, or changed framing.
        const FVector Movement(300.,-80.,0.);
        auto MovedData = MakeProjection(Rect, Center+Movement, Yaw);
        FVector MovedOffset;
        TestTrue(TEXT("Moving player is immediately solvable"), SolveOffset(MovedData, Desired, Center+Movement, MovedOffset));
        TestTrue(TEXT("Player motion does not change relative camera offset"), Offset.Equals(MovedOffset, 0.001));
        FVector RepeatOffset;
        TestTrue(TEXT("Repeated base-camera solve succeeds"), SolveOffset(MakeProjection(Rect,Center,Yaw), Desired, Center, RepeatOffset));
        TestTrue(TEXT("Stationary cursor does not feed back into camera"), RepeatOffset.Equals(Offset, 0.001));
    }
    AddInfo(FString::Printf(TEXT("Maximum actor projection error: %.9f pixels"), MaxError));
    const FIntRect Rect(0,0,1920,1080);
    FVector Offset(5,6,7);
    const auto Horizontal = MakeProjection(Rect,FVector::ZeroVector,0.,0.);
    TestFalse(TEXT("Parallel ray safely rejects framing"), SolveOffset(Horizontal,FVector2D(0.5,540./1079.),FVector::ZeroVector,Offset));
    TestEqual(TEXT("Failed solve clears world offset"),Offset,FVector::ZeroVector);
    FVector Aim;
    TestFalse(TEXT("Upward ray cannot aim at ground behind camera"),IntersectPlane(MakeProjection(Rect,FVector::ZeroVector,0.,45.),ToPixels(FVector2D(0.5,0.5),Rect),-5000.,Aim));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightScreenFramingInputTest, "CandyFight.Aiming.ScreenFramingInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightScreenFramingInputTest::RunTest(const FString& Parameters)
{
    using namespace CandyFightAimFraming;
    const FIntRect Rect(100,50,1380,770);
    TestEqual(TEXT("First pixel is reachable"),ClampCenter(FVector2D(-9999,-9999),Rect),FVector2D(100,50));
    TestEqual(TEXT("Last pixel is reachable"),ClampCenter(FVector2D(9999,9999),Rect),FVector2D(1379,769));
    FVector2D Cursor(1379,769);
    for(int32 I=0; I<100; ++I) Cursor=ClampCenter(Cursor+FVector2D(1000,1000),Rect);
    Cursor=ClampCenter(Cursor-FVector2D(1,1),Rect);
    TestEqual(TEXT("Outward motion never queues hidden cursor distance"),Cursor,FVector2D(1378,768));
    const FIntRect Resized(20,40,1940,1120);
    const FVector2D Ratio(.25,.75);
    const FVector2D Remapped=ToPixels(Normalize(ToPixels(Ratio,Rect),Rect),Resized);
    TestTrue(TEXT("Resize preserves relative position including viewport origin"),Normalize(Remapped,Resized).Equals(Ratio,1.e-9));
    TestTrue(TEXT("Corners are exact after resizing"),Normalize(ToPixels(FVector2D(1,1),Resized),Resized).Equals(FVector2D(1,1)));
    TestEqual(TEXT("Single-pixel view has a well-defined center"),Normalize(FVector2D(10,10),FIntRect(10,10,11,11)),FVector2D(.5,.5));
    TestTrue(TEXT("Right edge places player on left at ten percent"),Target(ToPixels(FVector2D(1,.5),Rect),Rect,FVector2D(.1,.1)).Equals(FVector2D(.1,.5)));
    TestTrue(TEXT("Upper right targets lower left"),Target(ToPixels(FVector2D(1,0),Rect),Rect,FVector2D(.1,.1)).Equals(FVector2D(.1,.9)));
    TestEqual(TEXT("Half-screen margin disables axis motion"),Target(ToPixels(FVector2D(1,0),Rect),Rect,FVector2D(.5,.5)),FVector2D(.5,.5));
    TestTrue(TEXT("Custom independent margins work"),Target(ToPixels(FVector2D(1,0),Rect),Rect,FVector2D(.2,.3)).Equals(FVector2D(.2,.7)));
    auto* Widget=NewObject<UCandyFightCrosshairWidget>();
    for(float Spread : {0.f,.5f,1.f})
    {
        Widget->SetSpread(Spread);
        Widget->SetScreenPosition(ClampCenter(FVector2D(9999,9999),Rect));
        TestEqual(TEXT("Spread does not pull cursor off edge"),Widget->ScreenPosition,FVector2D(1379,769));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightScreenFramingSmoothingTest, "CandyFight.Aiming.ScreenFramingSmoothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightScreenFramingSmoothingTest::RunTest(const FString& Parameters)
{
    const FVector2D Target(.1,.9);
    FCandyFightAimFramingState State;
    TestEqual(TEXT("Zero speed synchronizes immediately"),State.Update(Target,true,0.f,1.f/60,1),Target);
    State.Reset();
    const auto First=State.Update(Target,true,6.f,1.f/60,1);
    TestEqual(TEXT("Multiple camera evaluations only advance once"),State.Update(Target,true,6.f,1.f/60,1),First);
    TestEqual(TEXT("Paused game time does not advance smoothing"),State.Update(Target,true,6.f,0.f,2),First);
    TestEqual(TEXT("Disabled framing resets to center"),State.Update(Target,false,6.f,1.f/60,3),FVector2D(.5,.5));
    TestEqual(TEXT("Instant enable applies new target"),State.Update(Target,true,0.f,1.f/60,4),Target);
    FVector2D Results[3];
    int32 Index=0;
    for(int32 FPS : {30,60,120})
    {
        State.Reset();
        for(int32 Frame=0;Frame<FPS;++Frame) State.Update(Target,true,6.f,1.f/FPS,Frame);
        Results[Index++]=State.Position;
    }
    TestTrue(TEXT("30/60 FPS converge identically"),Results[0].Equals(Results[1],1.e-7));
    TestTrue(TEXT("60/120 FPS converge identically"),Results[1].Equals(Results[2],1.e-7));
    TestTrue(TEXT("Exponential smoothing matches elapsed game time"),Results[0].Equals(FMath::Lerp(FVector2D(.5,.5),Target,1.-FMath::Exp(-6.)),1.e-7));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightReleaseFramingTest, "CandyFight.Aiming.ReleaseFraming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightReleaseFramingTest::RunTest(const FString& Parameters)
{
    using namespace CandyFightAimFraming;
    const FVector2D Margins(.1,.1);
    const FVector2D OldTarget(.65,.65), NewTarget(.3,.4);
    const FVector Center(0,0,88);
    for (int32 FPS : {30,60,120})
    {
        FCandyFightAimFramingState State;
        State.Update(OldTarget, true, 0.f, 0.f, 0, Margins);
        State.BeginReleaseBlend(NewTarget, .20f);
        TestEqual(TEXT("Release does not change cached framing"), State.Position, OldTarget);
        TestEqual(TEXT("Same-frame evaluation retains camera framing"), State.Update(NewTarget,true,0.f,1.f,0,Margins), OldTarget);
        State.Update(NewTarget,true,0.f,0.f,1,Margins);
        FVector BeforeOffset, AfterOffset;
        const auto Projection = MakeProjection(FIntRect(0,0,1920,1080), Center);
        SolveOffset(Projection,OldTarget,Center,BeforeOffset);
        SolveOffset(Projection,State.Position,Center,AfterOffset);
        TestTrue(TEXT("Rebased input at release preserves actual camera location"), BeforeOffset.Equals(AfterOffset,.00001));
        FVector2D Previous = State.Position;
        const int32 Steps = FPS / 5;
        for (int32 I = 1; I <= Steps; ++I)
        {
            const uint64 Frame = I + 1;
            State.Update(NewTarget,true,0.f,1.f/FPS,Frame,Margins);
            const double T = double(I) / Steps;
            const FVector2D Expected = FMath::Lerp(OldTarget,NewTarget,T*T*(3.-2.*T));
            TestTrue(TEXT("Release follows elapsed-time SmoothStep at any tested frame rate"), State.Position.Equals(Expected,1.e-7));
            TestTrue(TEXT("Release moves monotonically towards the normal framing"),
                (State.Position-NewTarget).Size() <= (Previous-NewTarget).Size());
            const auto Once = State.Position;
            const double Elapsed = State.ReleaseElapsed;
            State.Update(FVector2D(.8,.8),true,0.f,1.f,Frame,Margins);
            TestEqual(TEXT("Repeated frame cannot retarget camera"), State.Position, Once);
            TestEqual(TEXT("Repeated frame cannot advance release time"), State.ReleaseElapsed, Elapsed);
            Previous = Once;
        }
        TestTrue(TEXT("Release completes after 0.20 seconds"), State.Position.Equals(NewTarget,1.e-7));
        TestEqual(TEXT("Completed release leaves no residual correction"), State.ReleaseSeconds, 0.);
    }
    FCandyFightAimFramingState State;
    State.Update(OldTarget,true,0.f,0.f,0,Margins);
    State.BeginReleaseBlend(NewTarget,.20f);
    auto WithMouse = State;
    const FVector2D MouseFramingDelta(.02,-.03);
    State.Update(NewTarget,true,0.f,.05f,1,Margins);
    WithMouse.Update(NewTarget+MouseFramingDelta,true,0.f,.05f,1,Margins);
    TestTrue(TEXT("New mouse input retains immediate framing response during release"),
        (WithMouse.Position-State.Position).Equals(MouseFramingDelta,1.e-9));
    const auto Interrupted = State.Position;
    State.BeginReleaseBlend(FVector2D(.75,.2),.20f);
    State.Update(FVector2D(.75,.2),true,0.f,0.f,2,Margins);
    TestTrue(TEXT("Consecutive releases start from current camera framing"), State.Position.Equals(Interrupted,1.e-9));
    State.Update(FVector2D(.9,.1),true,0.f,.01f,3,Margins);
    TestTrue(TEXT("Compensated framing respects configured edge margins"), State.Position.X <= .9 && State.Position.Y >= .1);
    State.Update(NewTarget,false,0.f,.01f,4,Margins);
    TestEqual(TEXT("Disabling framing clears release compensation"), State.ReleaseSeconds,0.);
    TestEqual(TEXT("Disabled framing centers camera"), State.Position,FVector2D(.5,.5));
    State.Update(OldTarget,true,0.f,0.f,5,Margins);
    State.BeginReleaseBlend(NewTarget,0.f);
    TestEqual(TEXT("Zero release duration retains instant response"),State.Update(NewTarget,true,0.f,0.f,6,Margins),NewTarget);
    State.BeginReleaseBlend(OldTarget,.2f);
    State.Reset();
    TestEqual(TEXT("Restart clears release transition"),State.ReleaseSeconds,0.);
    TestEqual(TEXT("Restart clears frame stamp"),State.LastFrame,MAX_uint64);
    for (const FIntRect& Rect : {FIntRect(0,0,1920,1080),FIntRect(70,90,1350,810),FIntRect(0,0,3840,2160)})
    {
        State.Reset();
        const auto Old = Target(ToPixels(FVector2D(.3,.3),Rect),Rect,Margins);
        const auto New = Target(ToPixels(FVector2D(.8,.7),Rect),Rect,Margins);
        State.Update(Old,true,0.f,0.f,0,Margins);
        State.BeginReleaseBlend(New,.2f);
        State.Update(New,true,0.f,.1f,1,Margins);
        TestTrue(TEXT("Normalized release is independent of viewport size and origin"),State.Position.Equals((Old+New)*.5,1.e-7));
    }
    return true;
}

#endif
