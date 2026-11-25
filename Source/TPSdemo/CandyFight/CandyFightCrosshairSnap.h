#pragma once

#include "CoreMinimal.h"

struct FCandyFightCrosshairSnapState
{
    FVector2D RawPosition = FVector2D::ZeroVector;
    FVector2D DisplayPosition = FVector2D::ZeroVector;
    FVector2D InputDelta = FVector2D::ZeroVector;
    FIntRect ViewRect;
    uint64 LastInputFrame = MAX_uint64;
    uint64 LastSnapFrame = MAX_uint64;
    bool bInitialized = false;
    bool bSnapping = false;
    bool bReturning = false;
    float ReturnSeconds = 0.f;

    void Reset();
    void UpdateInput(const FVector2D& RawInput, const FVector2D& MouseDelta, const FIntRect& Rect, uint64 Frame);
    void SnapTo(const FVector2D& Target, float Seconds, float DeltaTime, uint64 Frame);
    void Release();
    void ReturnToInput(float DeltaTime, uint64 Frame);
};
