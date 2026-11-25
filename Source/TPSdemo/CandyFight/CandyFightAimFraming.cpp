#include "CandyFight/CandyFightAimFraming.h"
#include "SceneView.h"

namespace CandyFightAimFraming
{
    FVector2D ToPixels(const FVector2D& Normalized, const FIntRect& Rect)
    {
        return FVector2D(Rect.Min) + Normalized * FVector2D(FMath::Max(0, Rect.Width() - 1), FMath::Max(0, Rect.Height() - 1));
    }

    FVector2D ClampCenter(const FVector2D& Position, const FIntRect& Rect)
    {
        if (Position.ContainsNaN()) return ToPixels(FVector2D(0.5, 0.5), Rect);
        return FVector2D(FMath::Clamp(Position.X, double(Rect.Min.X), double(FMath::Max(Rect.Min.X, Rect.Max.X - 1))),
            FMath::Clamp(Position.Y, double(Rect.Min.Y), double(FMath::Max(Rect.Min.Y, Rect.Max.Y - 1))));
    }

    FVector2D Normalize(const FVector2D& Position, const FIntRect& Rect)
    {
        const FVector2D Clamped = ClampCenter(Position, Rect) - FVector2D(Rect.Min);
        return FVector2D(Rect.Width() > 1 ? Clamped.X / (Rect.Width() - 1) : 0.5,
            Rect.Height() > 1 ? Clamped.Y / (Rect.Height() - 1) : 0.5);
    }

    FVector2D Target(const FVector2D& Position, const FIntRect& Rect, const FVector2D& Margins)
    {
        const FVector2D Direction = Normalize(Position, Rect) * 2.0 - FVector2D(1.0, 1.0);
        return FVector2D(0.5, 0.5) - Direction * (FVector2D(0.5, 0.5) - Margins);
    }

    bool DeprojectRay(const FSceneViewProjectionData& Projection, const FVector2D& Pixel, FVector& OutOrigin, FVector& OutDirection)
    {
        const FIntRect Rect = Projection.GetConstrainedViewRect();
        if (Rect.Width() <= 0 || Rect.Height() <= 0 || Pixel.ContainsNaN()) return false;
        // Engine DeprojectScreenToWorld truncates fractional pixels. Continuous framing
        // needs the original subpixel coordinate, or it visibly steps during smoothing.
        const double X = (Pixel.X - Rect.Min.X) / Rect.Width() * 2.0 - 1.0;
        const double Y = 1.0 - (Pixel.Y - Rect.Min.Y) / Rect.Height() * 2.0;
        const FMatrix Inverse = Projection.ComputeViewProjectionMatrix().Inverse();
        const FVector4 Near = Inverse.TransformFVector4(FVector4(X, Y, 1.0, 1.0));
        const FVector4 Far = Inverse.TransformFVector4(FVector4(X, Y, 0.01, 1.0));
        if (!FMath::IsFinite(Near.W) || !FMath::IsFinite(Far.W) || FMath::Abs(Near.W) < UE_SMALL_NUMBER || FMath::Abs(Far.W) < UE_SMALL_NUMBER) return false;
        OutOrigin = FVector(Near) / Near.W;
        OutDirection = (FVector(Far) / Far.W - OutOrigin).GetSafeNormal();
        return !OutOrigin.ContainsNaN() && !OutDirection.ContainsNaN() && !OutDirection.IsNearlyZero();
    }

    bool IntersectPlane(const FSceneViewProjectionData& Projection, const FVector2D& Pixel, double PlaneZ, FVector& OutPoint)
    {
        FVector Origin, Direction;
        if (!DeprojectRay(Projection, Pixel, Origin, Direction) || FMath::Abs(Direction.Z) < 0.0001) return false;
        const double Distance = (PlaneZ - Origin.Z) / Direction.Z;
        if (!FMath::IsFinite(Distance) || Distance < 0.0) return false;
        OutPoint = Origin + Direction * Distance;
        return !OutPoint.ContainsNaN();
    }

    bool SolveOffset(const FSceneViewProjectionData& BaseProjection, const FVector2D& TargetNormalized,
        const FVector& ActorCenter, FVector& OutOffset)
    {
        OutOffset = FVector::ZeroVector;
        FVector Point;
        if (!IntersectPlane(BaseProjection, ToPixels(TargetNormalized, BaseProjection.GetConstrainedViewRect()), ActorCenter.Z, Point)) return false;
        OutOffset = ActorCenter - Point;
        OutOffset.Z = 0.0;
        if (OutOffset.ContainsNaN())
        {
            OutOffset = FVector::ZeroVector;
            return false;
        }
        return true;
    }
}

void FCandyFightAimFramingState::Reset()
{
    *this = FCandyFightAimFramingState();
}

void FCandyFightAimFramingState::ClearReleaseBlend()
{
    ReleaseOffset = FVector2D::ZeroVector;
    ReleaseElapsed = ReleaseSeconds = 0.;
}

void FCandyFightAimFramingState::BeginReleaseBlend(const FVector2D& RebasedTarget, float Seconds)
{
    ClearReleaseBlend();
    if (LastFrame == MAX_uint64 || RebasedTarget.ContainsNaN() || !FMath::IsFinite(Seconds) || Seconds <= 0.f) return;
    // Start from the rendered framing, including any unfinished previous release.
    ReleaseOffset = Position - RebasedTarget;
    ReleaseSeconds = Seconds;
}

FVector2D FCandyFightAimFramingState::Update(const FVector2D& Target, bool bEnabled, float Speed, float DeltaTime, uint64 Frame,
    const FVector2D& Margins)
{
    if (!bEnabled)
    {
        ClearReleaseBlend();
        Position = FVector2D(0.5, 0.5);
        LastFrame = Frame;
    }
    else if (LastFrame != Frame)
    {
        const double Step = FMath::IsFinite(DeltaTime) ? FMath::Max(0.0f, DeltaTime) : 0.;
        FVector2D EffectiveTarget = Target;
        if (ReleaseSeconds > 0.)
        {
            ReleaseElapsed = FMath::Min(ReleaseElapsed + Step, ReleaseSeconds);
            const double T = ReleaseElapsed / ReleaseSeconds;
            EffectiveTarget += ReleaseOffset * (1. - T * T * (3. - 2. * T));
            if (ReleaseElapsed >= ReleaseSeconds) ClearReleaseBlend();
        }
        EffectiveTarget.X = FMath::Clamp(EffectiveTarget.X, Margins.X, 1. - Margins.X);
        EffectiveTarget.Y = FMath::Clamp(EffectiveTarget.Y, Margins.Y, 1. - Margins.Y);
        const double Alpha = 1.0 - FMath::Exp(-double(Speed) * Step);
        Position = Speed > 0.0f ? FMath::Lerp(Position, EffectiveTarget, Alpha) : EffectiveTarget;
        Position.X = FMath::Clamp(Position.X, Margins.X, 1. - Margins.X);
        Position.Y = FMath::Clamp(Position.Y, Margins.Y, 1. - Margins.Y);
        LastFrame = Frame;
    }
    return Position;
}
