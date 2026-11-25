#pragma once

#include "CoreMinimal.h"

struct FSceneViewProjectionData;

// Pixel-center coordinates throughout; ViewRect.Max is exclusive.
namespace CandyFightAimFraming
{
    FVector2D ClampCenter(const FVector2D& Position, const FIntRect& Rect);
    FVector2D Normalize(const FVector2D& Position, const FIntRect& Rect);
    FVector2D ToPixels(const FVector2D& Normalized, const FIntRect& Rect);
    FVector2D Target(const FVector2D& Position, const FIntRect& Rect, const FVector2D& Margins);
    bool DeprojectRay(const FSceneViewProjectionData& Projection, const FVector2D& Pixel, FVector& OutOrigin, FVector& OutDirection);
    bool IntersectPlane(const FSceneViewProjectionData& Projection, const FVector2D& Pixel, double PlaneZ, FVector& OutPoint);
    bool SolveOffset(const FSceneViewProjectionData& BaseProjection, const FVector2D& TargetNormalized,
        const FVector& ActorCenter, FVector& OutOffset);
}

struct FCandyFightAimFramingState
{
    FVector2D Position = FVector2D(0.5, 0.5);
    uint64 LastFrame = MAX_uint64;
    FVector2D ReleaseOffset = FVector2D::ZeroVector;
    double ReleaseElapsed = 0.;
    double ReleaseSeconds = 0.;

    void Reset();
    void ClearReleaseBlend();
    void BeginReleaseBlend(const FVector2D& RebasedTarget, float Seconds);
    FVector2D Update(const FVector2D& Target, bool bEnabled, float Speed, float DeltaTime, uint64 Frame,
        const FVector2D& Margins = FVector2D::ZeroVector);
};
