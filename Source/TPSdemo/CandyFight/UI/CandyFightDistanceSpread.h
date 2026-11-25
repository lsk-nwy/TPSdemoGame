#pragma once

#include "CoreMinimal.h"

namespace CandyFightDistanceSpread
{
    // Project the horizontal fan at the aiming distance, without camera feedback.
    TPSDEMO_API bool ProjectRadius(const FMatrix& ViewProjection, const FIntRect& Rect,
        const FVector& Muzzle, const FVector& Aim, float HalfAngleDegrees, double& OutPixels);
    TPSDEMO_API double TargetGap(double RadiusPixels, double MinGap, double MaxGap, double Multiplier);
    TPSDEMO_API double DesignerOffset(double GapPixels, double AuthoredGap, double Dpi, bool bPixelPerfect);
}

/** Exponential game-time smoothing; repeated updates in a frame never advance twice. */
struct FCandyFightDistanceSpreadState
{
    double Gap = 6.;
    double LastTime = 0.;
    uint64 LastFrame = MAX_uint64;
    bool bInitialized = false;
    void Reset() { bInitialized = false; LastFrame = MAX_uint64; }
    double Update(double Target, double Seconds, double Now, uint64 Frame);
};
