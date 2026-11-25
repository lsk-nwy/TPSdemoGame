#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class UCandyFightWeaponConfig;
class UWorld;
struct FSceneViewProjectionData;

struct FCandyFightAimAssistOutline
{
    TArray<FVector2D> Points;
    bool bSelected = false;
};

struct FCandyFightAimAssistDebug
{
    TArray<FCandyFightAimAssistOutline> Outlines;
    double Padding = 0.;
    double ReleasePadding = 0.;
    FVector2D RawCursor = FVector2D::ZeroVector;
    FVector2D DisplayCursor = FVector2D::ZeroVector;
    FVector2D SnapCenter = FVector2D::ZeroVector;
    bool bHasSnapCenter = false;
    FVector2D Muzzle = FVector2D::ZeroVector;
    FVector2D RawAim = FVector2D::ZeroVector;
    FVector2D CombatAim = FVector2D::ZeroVector;
    bool bHasDirections = false;
};

namespace CandyFightAimAssist
{
    struct FCandidate
    {
        TWeakObjectPtr<AActor> Actor;
        FVector Aim = FVector::ZeroVector;
        FVector VisiblePoint = FVector::ZeroVector;
        FVector2D ScreenAim = FVector2D::ZeroVector;
        double CenterDistance = 0.;
        double CameraDistance = 0.;
        bool bDirect = false;
    };

    TPSDEMO_API UPrimitiveComponent* GetEligibleBody(AActor* Actor);
    TPSDEMO_API bool HorizontalCenter(UPrimitiveComponent* Body, double Height, FVector& OutPoint);
    TPSDEMO_API bool ProjectBody(UPrimitiveComponent* Body, const FSceneViewProjectionData& Projection,
        TArray<FVector2D>& OutHull, FVector2D& OutCenter);
    TPSDEMO_API double DistanceToHull(const TArray<FVector2D>& Hull, const FVector2D& Point, FVector2D& OutNearest);
    TPSDEMO_API bool IsWithinRegion(AActor* Target, const FSceneViewProjectionData& Projection,
        const FVector2D& Cursor, float PaddingPixels);
    TPSDEMO_API int32 Select(const TArray<FCandidate>& Candidates, AActor* Previous, double SwitchBias);
    TPSDEMO_API bool HasShotPath(UWorld* World, AActor* Shooter, AActor* Gun, AActor* Target,
        const FVector& Muzzle, float Range, FVector& OutAim);
    TPSDEMO_API bool HasVisiblePath(UWorld* World, AActor* Shooter, AActor* Gun, AActor* Target,
        const FVector& Camera, const FVector& BodyPoint);
    TPSDEMO_API bool Evaluate(UWorld* World, AActor* Shooter, AActor* Gun, AActor* Target, const FVector& Muzzle,
        const FSceneViewProjectionData& Projection, const FVector2D& Cursor,
        const UCandyFightWeaponConfig& Config, FCandidate& OutCandidate, TArray<FVector2D>* OutHull = nullptr, bool bRetain = false);
    TPSDEMO_API bool Resolve(UWorld* World, AActor* Shooter, AActor* Gun, const FVector& Muzzle,
        const FSceneViewProjectionData& Projection, const FVector2D& Cursor,
        const UCandyFightWeaponConfig& Config, AActor* Previous, FCandidate& OutCandidate,
        FCandyFightAimAssistDebug* Debug = nullptr, AActor* Excluded = nullptr);
    TPSDEMO_API bool IsDebugEnabled();
}
