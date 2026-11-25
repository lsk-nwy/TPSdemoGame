#include "CandyFight/CandyFightCrosshairSnap.h"
#include "CandyFight/CandyFightAimFraming.h"

void FCandyFightCrosshairSnapState::Reset()
{
    *this = FCandyFightCrosshairSnapState();
}

void FCandyFightCrosshairSnapState::UpdateInput(const FVector2D& RawInput, const FVector2D& MouseDelta,
    const FIntRect& Rect, uint64 Frame)
{
    using namespace CandyFightAimFraming;
    if (Rect.Width() <= 0 || Rect.Height() <= 0) return;
    if (!bInitialized)
    {
        RawPosition = DisplayPosition = ClampCenter(RawInput, Rect);
        ViewRect = Rect;
        bInitialized = true;
    }
    else if (ViewRect != Rect)
    {
        RawPosition = ToPixels(Normalize(RawInput, ViewRect), Rect);
        DisplayPosition = ToPixels(Normalize(DisplayPosition, ViewRect), Rect);
        ViewRect = Rect;
    }
    else if (LastInputFrame != Frame)
    {
        RawPosition = ClampCenter(RawInput, Rect);
    }
    if (LastInputFrame != Frame)
    {
        LastInputFrame = Frame;
        InputDelta = MouseDelta.ContainsNaN() ? FVector2D::ZeroVector : MouseDelta;
        RawPosition = ClampCenter(RawPosition + InputDelta, Rect);
        DisplayPosition = ClampCenter(DisplayPosition + InputDelta, Rect);
    }
    if (!bSnapping && !bReturning) DisplayPosition = RawPosition;
}

void FCandyFightCrosshairSnapState::SnapTo(const FVector2D& Target, float Seconds, float DeltaTime, uint64 Frame)
{
    if (!bInitialized || LastSnapFrame == Frame || Target.ContainsNaN() ||
        !FMath::IsFinite(Seconds) || Seconds < 0.f || !FMath::IsFinite(DeltaTime)) return;
    LastSnapFrame = Frame;
    bSnapping = true;
    bReturning = false;
    ReturnSeconds = Seconds;
    const double Alpha = Seconds > 0.f ? 1. - FMath::Exp(-FMath::Loge(20.) * FMath::Max(0.f, DeltaTime) / Seconds) : 1.;
    DisplayPosition = CandyFightAimFraming::ClampCenter(FMath::Lerp(DisplayPosition, Target, Alpha), ViewRect);
}

void FCandyFightCrosshairSnapState::Release()
{
    if (!bSnapping) return;
    bSnapping = false;
    bReturning = ReturnSeconds > 0.f;
    // Preserve both input and display on release; start catching up on the next frame.
    LastSnapFrame = LastInputFrame;
    if (!bReturning) DisplayPosition = RawPosition;
}

void FCandyFightCrosshairSnapState::ReturnToInput(float DeltaTime, uint64 Frame)
{
    if (!bReturning || LastSnapFrame == Frame || !FMath::IsFinite(DeltaTime)) return;
    LastSnapFrame = Frame;
    const double Alpha = 1. - FMath::Exp(-FMath::Loge(20.) * FMath::Max(0.f, DeltaTime) / ReturnSeconds);
    DisplayPosition = FMath::Lerp(DisplayPosition, RawPosition, Alpha);
    if (DisplayPosition.Equals(RawPosition, .01))
    {
        DisplayPosition = RawPosition;
        bReturning = false;
    }
}
