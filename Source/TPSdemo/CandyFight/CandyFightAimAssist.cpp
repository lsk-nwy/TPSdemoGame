#include "CandyFight/CandyFightAimAssist.h"
#include "CandyFight/CandyFightAimFraming.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "SceneView.h"

namespace
{
    TAutoConsoleVariable<int32> DebugAimAssist(TEXT("CandyFight.AimAssist.Debug"), 0,
        TEXT("Draw aim assist bodies, padding and raw/corrected directions."), ECVF_Cheat);

    double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

    TArray<FVector2D> ConvexHull(TArray<FVector2D> Points)
    {
        Points.Sort([](const auto& A, const auto& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
        TArray<FVector2D> Hull;
        for (const auto& P : Points)
        {
            while (Hull.Num() >= 2 && Cross(Hull.Last() - Hull[Hull.Num()-2], P - Hull.Last()) <= 0.) Hull.Pop();
            Hull.Add(P);
        }
        const int32 Lower = Hull.Num();
        for (int32 I = Points.Num() - 2; I >= 0; --I)
        {
            const auto& P = Points[I];
            while (Hull.Num() > Lower && Cross(Hull.Last() - Hull[Hull.Num()-2], P - Hull.Last()) <= 0.) Hull.Pop();
            Hull.Add(P);
        }
        if (Hull.Num() > 1) Hull.Pop();
        return Hull;
    }

    TArray<FVector> BoxCorners(const UBoxComponent& Box)
    {
        TArray<FVector> Points;
        const FVector E = Box.GetUnscaledBoxExtent();
        for (int32 I = 0; I < 8; ++I)
            Points.Add(Box.GetComponentTransform().TransformPosition(FVector(
                I & 1 ? E.X : -E.X, I & 2 ? E.Y : -E.Y, I & 4 ? E.Z : -E.Z)));
        return Points;
    }

    // A perspective capsule silhouette is the convex hull of its two sphere silhouettes.
    bool AddSphereSilhouette(const FVector& Center, double Radius, const FVector& Camera, TArray<FVector>& Points)
    {
        const FVector ToCenter = Center - Camera;
        const double Distance = ToCenter.Size();
        if (Distance <= Radius + 1.) return false;
        const FVector Axis = ToCenter / Distance;
        FVector U, V;
        Axis.FindBestAxisVectors(U, V);
        const FVector RingCenter = Center - Axis * (Radius * Radius / Distance);
        const double RingRadius = Radius * FMath::Sqrt(1. - FMath::Square(Radius / Distance));
        for (int32 I = 0; I < 64; ++I)
        {
            const double A = 2. * UE_PI * I / 64.;
            Points.Add(RingCenter + RingRadius * (U * FMath::Cos(A) + V * FMath::Sin(A)));
        }
        return true;
    }

    // Match the projected primitive mathematically; physics tangent-ray queries can miss its outline.
    bool BodySurface(UPrimitiveComponent* Body, const FVector& Origin, const FVector& Direction, FVector& OutPoint)
    {
        double Nearest = TNumericLimits<double>::Max();
        if (const auto* Box = Cast<UBoxComponent>(Body))
        {
            const FVector LocalOrigin = Box->GetComponentTransform().InverseTransformPosition(Origin);
            const FVector LocalDirection = Box->GetComponentTransform().InverseTransformVector(Direction);
            const FVector Extent = Box->GetUnscaledBoxExtent();
            double Enter = 0., Exit = TNumericLimits<double>::Max();
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                if (FMath::Abs(LocalDirection[Axis]) < UE_SMALL_NUMBER)
                {
                    if (FMath::Abs(LocalOrigin[Axis]) > Extent[Axis]) return false;
                    continue;
                }
                double A = (-Extent[Axis] - LocalOrigin[Axis]) / LocalDirection[Axis];
                double B = (Extent[Axis] - LocalOrigin[Axis]) / LocalDirection[Axis];
                if (A > B) Swap(A, B);
                Enter = FMath::Max(Enter, A);
                Exit = FMath::Min(Exit, B);
                if (Enter > Exit) return false;
            }
            Nearest = Enter;
        }
        else if (const auto* Capsule = Cast<UCapsuleComponent>(Body))
        {
            const FVector Axis = Capsule->GetUpVector();
            const FVector O = Origin - Capsule->GetComponentLocation();
            const double Radius = Capsule->GetScaledCapsuleRadius();
            const double HalfLength = Capsule->GetScaledCapsuleHalfHeight() - Radius;
            const double OZ = FVector::DotProduct(O, Axis), DZ = FVector::DotProduct(Direction, Axis);
            const FVector OP = O - Axis * OZ, DP = Direction - Axis * DZ;
            const double A = DP.SizeSquared(), B = FVector::DotProduct(OP, DP), C = OP.SizeSquared() - Radius * Radius;
            const double Disc = B * B - A * C;
            if (A > UE_SMALL_NUMBER && Disc >= 0.)
            {
                for (double T : {(-B - FMath::Sqrt(Disc)) / A, (-B + FMath::Sqrt(Disc)) / A})
                    if (T >= 0. && FMath::Abs(OZ + DZ * T) <= HalfLength) Nearest = FMath::Min(Nearest, T);
            }
            for (double Sign : {-1., 1.})
            {
                const FVector SphereO = O - Axis * (Sign * HalfLength);
                const double SphereB = FVector::DotProduct(SphereO, Direction);
                const double SphereDisc = SphereB * SphereB - SphereO.SizeSquared() + Radius * Radius;
                if (SphereDisc < 0.) continue;
                for (double T : {-SphereB - FMath::Sqrt(SphereDisc), -SphereB + FMath::Sqrt(SphereDisc)})
                    if (T >= 0. && Sign * (OZ + DZ * T) >= HalfLength) Nearest = FMath::Min(Nearest, T);
            }
        }
        if (Nearest == TNumericLimits<double>::Max() || !FMath::IsFinite(Nearest)) return false;
        OutPoint = Origin + Direction * Nearest;
        return true;
    }
}

bool CandyFightAimAssist::IsDebugEnabled()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return DebugAimAssist.GetValueOnGameThread() != 0;
#endif
}

UPrimitiveComponent* CandyFightAimAssist::GetEligibleBody(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->IsHidden() ||
        !Actor->CanBeDamaged() || !Actor->GetActorEnableCollision()) return nullptr;
    UPrimitiveComponent* Body = nullptr;
    if (auto* Enemy = Cast<ACandyFightEnemyCharacter>(Actor))
    {
        if (Enemy->bDefeated || Enemy->Health <= 0.f) return nullptr;
        Body = Enemy->GetCapsuleComponent();
    }
    else if (auto* Target = Cast<ACandyFightTarget>(Actor))
    {
        if (Target->Health <= 0.f) return nullptr;
        Body = Target->HitBox;
    }
    return IsValid(Body) && Body->IsRegistered() && Body->IsQueryCollisionEnabled() &&
        Body->GetCollisionResponseToChannel(CandyFightWeapons::TraceChannel) == ECR_Block ? Body : nullptr;
}

bool CandyFightAimAssist::HorizontalCenter(UPrimitiveComponent* Body, double Height, FVector& OutPoint)
{
    if (!Body || !FMath::IsFinite(Height)) return false;
    if (const auto* Capsule = Cast<UCapsuleComponent>(Body))
    {
        OutPoint = Capsule->GetComponentLocation();
        OutPoint.Z = Height;
        const FVector Axis = Capsule->GetUpVector();
        const double Radius = Capsule->GetScaledCapsuleRadius();
        const double Length = Capsule->GetScaledCapsuleHalfHeight() - Radius;
        const FVector Center = Capsule->GetComponentLocation();
        const FVector Nearest = FMath::ClosestPointOnSegment(OutPoint, Center - Axis * Length, Center + Axis * Length);
        return (OutPoint - Nearest).SizeSquared() < FMath::Square(Radius);
    }
    const auto* Box = Cast<UBoxComponent>(Body);
    if (!Box) return false;
    const TArray<FVector> Corners = BoxCorners(*Box);
    TArray<FVector2D> Section;
    for (int32 I = 0; I < 8; ++I)
    {
        if (FMath::IsNearlyEqual(Corners[I].Z, Height, 0.001)) Section.AddUnique(FVector2D(Corners[I]));
        for (int32 Bit : {1, 2, 4})
        {
            const int32 J = I ^ Bit;
            if (J <= I || (Corners[I].Z - Height) * (Corners[J].Z - Height) >= 0.) continue;
            const double T = (Height - Corners[I].Z) / (Corners[J].Z - Corners[I].Z);
            Section.AddUnique(FVector2D(FMath::Lerp(Corners[I], Corners[J], T)));
        }
    }
    const auto Hull = ConvexHull(MoveTemp(Section));
    if (Hull.Num() < 3) return false;
    double Area = 0.;
    FVector2D Center = FVector2D::ZeroVector;
    for (int32 I = 0; I < Hull.Num(); ++I)
    {
        const FVector2D A = Hull[I], B = Hull[(I + 1) % Hull.Num()];
        const double Weight = Cross(A, B);
        Area += Weight;
        Center += (A + B) * Weight;
    }
    if (FMath::Abs(Area) < UE_SMALL_NUMBER) return false;
    Center /= 3. * Area;
    OutPoint = FVector(Center.X, Center.Y, Height);
    return !OutPoint.ContainsNaN();
}

bool CandyFightAimAssist::ProjectBody(UPrimitiveComponent* Body, const FSceneViewProjectionData& Projection,
    TArray<FVector2D>& OutHull, FVector2D& OutCenter)
{
    OutHull.Reset();
    TArray<FVector> Points;
    if (const auto* Box = Cast<UBoxComponent>(Body)) Points = BoxCorners(*Box);
    else if (const auto* Capsule = Cast<UCapsuleComponent>(Body))
    {
        const double Radius = Capsule->GetScaledCapsuleRadius();
        const FVector Offset = Capsule->GetUpVector() * (Capsule->GetScaledCapsuleHalfHeight() - Radius);
        if (!AddSphereSilhouette(Capsule->GetComponentLocation() - Offset, Radius, Projection.ViewOrigin, Points) ||
            !AddSphereSilhouette(Capsule->GetComponentLocation() + Offset, Radius, Projection.ViewOrigin, Points)) return false;
    }
    else return false;
    const FIntRect Rect = Projection.GetConstrainedViewRect();
    const FMatrix Matrix = Projection.ComputeViewProjectionMatrix();
    if (!FSceneView::ProjectWorldToScreen(Body->GetComponentLocation(), Rect, Matrix, OutCenter)) return false;
    TArray<FVector2D> Pixels;
    for (const auto& Point : Points)
    {
        FVector2D Pixel;
        if (!FSceneView::ProjectWorldToScreen(Point, Rect, Matrix, Pixel) || Pixel.ContainsNaN()) return false;
        Pixels.Add(Pixel);
    }
    OutHull = ConvexHull(MoveTemp(Pixels));
    return OutHull.Num() >= 3;
}

double CandyFightAimAssist::DistanceToHull(const TArray<FVector2D>& Hull, const FVector2D& Point, FVector2D& OutNearest)
{
    double Best = TNumericLimits<double>::Max();
    bool bInside = Hull.Num() >= 3;
    for (int32 I = 0; I < Hull.Num(); ++I)
    {
        const FVector2D A = Hull[I], B = Hull[(I + 1) % Hull.Num()], Edge = B - A;
        if (Cross(Edge, Point - A) < -0.00001) bInside = false;
        const double T = Edge.SizeSquared() > UE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / Edge.SizeSquared(), 0., 1.) : 0.;
        const FVector2D Nearest = A + Edge * T;
        const double Distance = (Point - Nearest).Size();
        if (Distance < Best) { Best = Distance; OutNearest = Nearest; }
    }
    if (bInside) { OutNearest = Point; return 0.; }
    return Best;
}

bool CandyFightAimAssist::IsWithinRegion(AActor* Target, const FSceneViewProjectionData& Projection,
    const FVector2D& Cursor, float PaddingPixels)
{
    auto* Body = GetEligibleBody(Target);
    TArray<FVector2D> Hull;
    FVector2D Center, Nearest;
    const FIntRect Rect = Projection.GetConstrainedViewRect();
    return Body && !Cursor.ContainsNaN() && Rect.Height() > 0 && FMath::IsFinite(PaddingPixels) && PaddingPixels >= 0.f &&
        ProjectBody(Body, Projection, Hull, Center) &&
        DistanceToHull(Hull, Cursor, Nearest) <= double(PaddingPixels) * Rect.Height() / 1080. + 0.00001;
}

int32 CandyFightAimAssist::Select(const TArray<FCandidate>& Candidates, AActor* Previous, double SwitchBias)
{
    int32 Best = INDEX_NONE, Old = INDEX_NONE;
    for (int32 I = 0; I < Candidates.Num(); ++I)
    {
        const auto& C = Candidates[I];
        if (C.Actor.Get() == Previous) Old = I;
        if (Best == INDEX_NONE || (C.bDirect && !Candidates[Best].bDirect) ||
            (C.bDirect == Candidates[Best].bDirect && (C.CenterDistance < Candidates[Best].CenterDistance ||
            (C.CenterDistance == Candidates[Best].CenterDistance && C.CameraDistance < Candidates[Best].CameraDistance)))) Best = I;
    }
    if (Old != INDEX_NONE && Best != INDEX_NONE && Old != Best &&
        Candidates[Old].bDirect == Candidates[Best].bDirect && SwitchBias > 0. &&
        Candidates[Best].CenterDistance + SwitchBias >= Candidates[Old].CenterDistance) return Old;
    return Best;
}

bool CandyFightAimAssist::HasShotPath(UWorld* World, AActor* Shooter, AActor* Gun, AActor* Target,
    const FVector& Muzzle, float Range, FVector& OutAim)
{
    UPrimitiveComponent* Body = GetEligibleBody(Target);
    if (!World || !Body || Muzzle.ContainsNaN() || !HorizontalCenter(Body, Muzzle.Z, OutAim)) return false;
    const FVector Delta = OutAim - Muzzle;
    const double Distance = Delta.Size();
    if (Distance < 5. || Distance > Range || !FMath::IsFinite(Range)) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightAimAssistShot), false, Shooter);
    if (Gun) Params.AddIgnoredActor(Gun);
    FVector Origin = Shooter ? Shooter->GetActorLocation() : Muzzle;
    Origin.Z = Muzzle.Z;
    FHitResult Hit;
    if (World->SweepSingleByChannel(Hit, Origin, Muzzle, FQuat::Identity, CandyFightWeapons::TraceChannel,
            FCollisionShape::MakeSphere(1.f), Params) ||
        World->OverlapBlockingTestByChannel(Muzzle, FQuat::Identity, CandyFightWeapons::TraceChannel,
            FCollisionShape::MakeSphere(1.f), Params)) return false;
    World->LineTraceSingleByChannel(Hit, Muzzle, OutAim, CandyFightWeapons::TraceChannel, Params);
    return Hit.bBlockingHit && !Hit.bStartPenetrating && Hit.GetActor() == Target;
}

bool CandyFightAimAssist::HasVisiblePath(UWorld* World, AActor* Shooter, AActor* Gun, AActor* Target,
    const FVector& Camera, const FVector& BodyPoint)
{
    if (!World || !GetEligibleBody(Target)) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightAimAssistVisibility), false, Shooter);
    Params.AddIgnoredActor(Target);
    if (Gun) Params.AddIgnoredActor(Gun);
    FHitResult Hit;
    return !World->LineTraceSingleByChannel(Hit, Camera, BodyPoint, ECC_Visibility, Params);
}

bool CandyFightAimAssist::Evaluate(UWorld* World, AActor* Shooter, AActor* Gun, AActor* Target, const FVector& Muzzle,
    const FSceneViewProjectionData& Projection, const FVector2D& Cursor,
    const UCandyFightWeaponConfig& Config, FCandidate& OutCandidate, TArray<FVector2D>* OutHull, bool bRetain)
{
    OutCandidate = FCandidate();
    UPrimitiveComponent* Body = GetEligibleBody(Target);
    const FIntRect Rect = Projection.GetConstrainedViewRect();
    if (!World || !Body || Target == Shooter || !Config.bEnableAimAssist || Cursor.ContainsNaN() || Rect.Height() <= 0 ||
        !FMath::IsFinite(Config.AimAssistPaddingPixels) || Config.AimAssistPaddingPixels < 0.f ||
        !FMath::IsFinite(Config.AimAssistSwitchBiasPixels) || Config.AimAssistSwitchBiasPixels < 0.f ||
        !FMath::IsFinite(Config.AimAssistSnapSeconds) || Config.AimAssistSnapSeconds < 0.f ||
        !FMath::IsFinite(Config.AimAssistReleasePaddingPixels) || Config.AimAssistReleasePaddingPixels < 0.f) return false;
    if ((Body->GetComponentLocation() - Muzzle).Size() > Config.Range + Body->Bounds.SphereRadius) return false;
    TArray<FVector2D> Hull;
    FVector2D Center, Nearest;
    if (!ProjectBody(Body, Projection, Hull, Center)) return false;
    if (OutHull) *OutHull = Hull;
    const double Distance = DistanceToHull(Hull, Cursor, Nearest);
    const double Padding = (double(Config.AimAssistPaddingPixels) + (bRetain ? Config.AimAssistReleasePaddingPixels : 0.)) * Rect.Height() / 1080.;
    if (Distance > Padding + 0.00001) return false;
    if (!HasShotPath(World, Shooter, Gun, Target, Muzzle, Config.Range, OutCandidate.Aim)) return false;
    // Nudge tangent rays inside the silhouette to avoid floating-point contact ambiguity.
    const FVector2D SurfacePixel = Nearest + (Center - Nearest).GetSafeNormal() * 0.05;
    FVector RayStart, RayDirection, Surface;
    if (!CandyFightAimFraming::DeprojectRay(Projection, SurfacePixel, RayStart, RayDirection) ||
        !BodySurface(Body, RayStart, RayDirection, Surface) ||
        !HasVisiblePath(World, Shooter, Gun, Target, Projection.ViewOrigin, Surface)) return false;
    if (!FSceneView::ProjectWorldToScreen(OutCandidate.Aim, Rect, Projection.ComputeViewProjectionMatrix(), OutCandidate.ScreenAim) ||
        OutCandidate.ScreenAim.ContainsNaN() || OutCandidate.ScreenAim.X < Rect.Min.X || OutCandidate.ScreenAim.X > Rect.Max.X - 1 ||
        OutCandidate.ScreenAim.Y < Rect.Min.Y || OutCandidate.ScreenAim.Y > Rect.Max.Y - 1 ||
        !CandyFightAimFraming::DeprojectRay(Projection, OutCandidate.ScreenAim, RayStart, RayDirection) ||
        !BodySurface(Body, RayStart, RayDirection, Surface) ||
        !HasVisiblePath(World, Shooter, Gun, Target, Projection.ViewOrigin, Surface)) return false;
    OutCandidate.Actor = Target;
    OutCandidate.VisiblePoint = Surface;
    OutCandidate.bDirect = Distance <= 0.00001;
    OutCandidate.CenterDistance = (Cursor - Center).Size();
    OutCandidate.CameraDistance = (Body->GetComponentLocation() - Projection.ViewOrigin).SizeSquared();
    return true;
}

bool CandyFightAimAssist::Resolve(UWorld* World, AActor* Shooter, AActor* Gun, const FVector& Muzzle,
    const FSceneViewProjectionData& Projection, const FVector2D& Cursor,
    const UCandyFightWeaponConfig& Config, AActor* Previous, FCandidate& OutCandidate, FCandyFightAimAssistDebug* Debug, AActor* Excluded)
{
    OutCandidate = FCandidate();
    if (Debug) *Debug = FCandyFightAimAssistDebug();
    const FIntRect Rect = Projection.GetConstrainedViewRect();
    if (!World || !Config.bEnableAimAssist || Cursor.ContainsNaN() || Rect.Height() <= 0 ||
        !FMath::IsFinite(Config.AimAssistPaddingPixels) || Config.AimAssistPaddingPixels < 0.f ||
        !FMath::IsFinite(Config.AimAssistSwitchBiasPixels) || Config.AimAssistSwitchBiasPixels < 0.f) return false;
    const double Scale = Rect.Height() / 1080.;
    const double Padding = Config.AimAssistPaddingPixels * Scale;
    if (Debug) { Debug->Padding = Padding; Debug->ReleasePadding = Padding + double(Config.AimAssistReleasePaddingPixels) * Scale; }
    TArray<FCandidate> Candidates;
    TArray<int32> OutlineIndices;
    auto AddCandidate = [&](AActor* Actor)
    {
        if (Actor == Excluded) return;
        FCandidate Candidate;
        TArray<FVector2D> Hull;
        const bool bValid = Evaluate(World, Shooter, Gun, Actor, Muzzle, Projection, Cursor, Config, Candidate, Debug ? &Hull : nullptr, Actor == Previous);
        int32 Outline = INDEX_NONE;
        if (Debug && Hull.Num() >= 3) { Outline = Debug->Outlines.AddDefaulted(); Debug->Outlines[Outline].Points = MoveTemp(Hull); }
        if (!bValid) return;
        Candidates.Add(Candidate);
        OutlineIndices.Add(Outline);
    };
    for (TActorIterator<ACandyFightEnemyCharacter> It(World); It; ++It) AddCandidate(*It);
    for (TActorIterator<ACandyFightTarget> It(World); It; ++It) AddCandidate(*It);
    const int32 Selected = Select(Candidates, Previous, Config.AimAssistSwitchBiasPixels * Scale);
    if (Selected == INDEX_NONE) return false;
    OutCandidate = Candidates[Selected];
    if (Debug && OutlineIndices[Selected] != INDEX_NONE) Debug->Outlines[OutlineIndices[Selected]].bSelected = true;
    return true;
}
