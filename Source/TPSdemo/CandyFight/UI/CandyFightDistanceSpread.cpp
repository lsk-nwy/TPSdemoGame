#include "CandyFight/UI/CandyFightDistanceSpread.h"
#include "SceneView.h"

bool CandyFightDistanceSpread::ProjectRadius(const FMatrix& ViewProjection, const FIntRect& Rect,
    const FVector& Muzzle, const FVector& Aim, float HalfAngleDegrees, double& OutPixels)
{
    OutPixels = 0.;
    if (Rect.Width() <= 0 || Rect.Height() <= 0 || ViewProjection.ContainsNaN() ||
        Muzzle.ContainsNaN() || Aim.ContainsNaN() || !FMath::IsFinite(HalfAngleDegrees) ||
        HalfAngleDegrees < 0.f || HalfAngleDegrees > 45.f) return false;
    const FVector Delta(Aim.X - Muzzle.X, Aim.Y - Muzzle.Y, 0.);
    const double Distance = Delta.Size();
    if (Distance <= UE_DOUBLE_SMALL_NUMBER || HalfAngleDegrees == 0.f) return true;
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Delta / Distance);
    const double Radius = Distance * FMath::Tan(FMath::DegreesToRadians(double(HalfAngleDegrees)));
    const FVector Center(Aim.X, Aim.Y, Muzzle.Z);
    const FVector Edge = Side * Radius;
    FVector2D ScreenCenter, Left, Right;
    if (!FSceneView::ProjectWorldToScreen(Center, Rect, ViewProjection, ScreenCenter) ||
        !FSceneView::ProjectWorldToScreen(Center - Edge, Rect, ViewProjection, Left) ||
        !FSceneView::ProjectWorldToScreen(Center + Edge, Rect, ViewProjection, Right)) return false;
    // The larger side also handles perspective asymmetry near viewport edges.
    OutPixels = FMath::Max((Left - ScreenCenter).Size(), (Right - ScreenCenter).Size());
    return FMath::IsFinite(OutPixels);
}

double CandyFightDistanceSpread::TargetGap(double RadiusPixels, double MinGap, double MaxGap, double Multiplier)
{
    const double Min = FMath::IsFinite(MinGap) ? FMath::Max(0., MinGap) : 6.;
    const double Max = FMath::IsFinite(MaxGap) ? FMath::Max(Min, MaxGap) : Min;
    if (!FMath::IsFinite(RadiusPixels) || !FMath::IsFinite(Multiplier)) return Min;
    return FMath::Clamp(FMath::Max(0., RadiusPixels) * FMath::Max(0., Multiplier), Min, Max);
}

double FCandyFightDistanceSpreadState::Update(double Target, double Seconds, double Now, uint64 Frame)
{
    if (!FMath::IsFinite(Target) || !FMath::IsFinite(Now)) return Gap;
    if (LastFrame == Frame) return Gap;
    LastFrame = Frame;
    if (!bInitialized || Now < LastTime || !FMath::IsFinite(Seconds) || Seconds <= 0.) Gap = Target;
    else
    {
        // 95% of a fixed-target transition in Seconds; independent of frame rate.
        const double Alpha = 1. - FMath::Exp(-3. * FMath::Max(0., Now - LastTime) / Seconds);
        Gap = FMath::Lerp(Gap, Target, Alpha);
        if (FMath::Abs(Gap - Target) < .001) Gap = Target;
    }
    bInitialized = true;
    LastTime = Now;
    return Gap;
}

double CandyFightDistanceSpread::DesignerOffset(double GapPixels, double AuthoredGap, double Dpi, bool bPixelPerfect)
{
    return GapPixels / (bPixelPerfect ? 1. : FMath::Max(Dpi, double(UE_SMALL_NUMBER))) - FMath::Max(0., AuthoredGap);
}
