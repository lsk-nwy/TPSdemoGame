#include "CandyFightCombatPositioning.h"
#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Engine/World.h"

bool CandyFightPositioning::InsideLeash(const FVector& Point, const FVector& Anchor, float Radius)
{
    return !Point.ContainsNaN() && !Anchor.ContainsNaN() && FVector::DistSquared2D(Point, Anchor) <= FMath::Square(Radius);
}
float CandyFightPositioning::PositionAcceptanceRadius(const ACandyFightEnemyCharacter& Enemy, float MinDistance, float MaxDistance)
{
    return FMath::Min3(Enemy.EnemyConfig->DestinationTolerance, 10.f, FMath::Max(0.f, (MaxDistance-MinDistance)*.1f));
}
bool CandyFightPositioning::FindPath(ACandyFightEnemyCharacter& Enemy, const FVector& Candidate, const FVector& Anchor,
    float Radius, FVector& Projected, FNavPathSharedPtr& Path, bool bConstrainLeash, FString* FailureReason)
{
    Path.Reset();
    if (FailureReason) FailureReason->Reset();
    auto Fail = [FailureReason](const TCHAR* Reason) { if (FailureReason) *FailureReason = Reason; return false; };
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy.GetWorld());
    if (!Nav) return Fail(TEXT("导航系统不可用"));
    if (Candidate.ContainsNaN()) return Fail(TEXT("目标坐标无效"));
    const auto& Props = Enemy.GetNavAgentPropertiesRef();
    FNavLocation Point;
    const float Extent = Enemy.GetPatrolConfig().ProjectionDistance;
    if (!Nav->ProjectPointToNavigation(Candidate, Point, FVector(Extent), &Props) ||
        FVector::DistSquared(Candidate, Point.Location) > FMath::Square(Extent)) return Fail(TEXT("目标附近没有合适导航面"));
    if (bConstrainLeash && !InsideLeash(Point.Location, Anchor, Radius)) return Fail(TEXT("导航目标超出追踪范围"));
    const auto* Data = Nav->GetNavDataForProps(Props, Enemy.GetNavAgentLocation());
    if (!Data) return Fail(TEXT("没有匹配敌人尺寸的导航数据"));
    FPathFindingQuery Query(&Enemy, *Data, Enemy.GetNavAgentLocation(), Point.Location);
    Query.SetAllowPartialPaths(false);
    auto Found = Nav->FindPathSync(Props, Query);
    if (!Found.IsSuccessful() || !Found.Path.IsValid() || Found.Path->IsPartial()) return Fail(TEXT("没有完整可达路径"));
    if (bConstrainLeash)
        for (const FNavPathPoint& P : Found.Path->GetPathPoints()) if (!InsideLeash(P.Location, Anchor, Radius)) return Fail(TEXT("路径绕出追踪范围"));
    // 不让路径在后台偷偷改成绕出追踪圈的路线；失效后由控制器重新校验。
    Found.Path->EnableRecalculationOnInvalidation(false);
    Projected = Point.Location; Path = Found.Path;
    return true;
}
bool CandyFightPositioning::FindCluePath(ACandyFightEnemyCharacter& Enemy, const FVector& Clue, const FVector& Anchor,
    float Radius, FVector& Projected, FNavPathSharedPtr& Path, FString* FailureReason)
{
    Path.Reset();
    if (FailureReason) FailureReason->Reset();
    auto Fail = [FailureReason](const TCHAR* Reason) { if (FailureReason) *FailureReason = Reason; return false; };
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy.GetWorld());
    if (!Nav) return Fail(TEXT("导航系统不可用"));
    if (Clue.ContainsNaN()) return Fail(TEXT("线索坐标无效"));
    if (!InsideLeash(Clue, Anchor, Radius)) return Fail(TEXT("线索超出追踪范围"));
    // 横向偏移仍受原导航容差限制；高度额外允许一个角色身高，不能把胸口/枪口高度算作横向误差。
    // 不修改线索，也不读取目标当前坐标。高差仍有界，避免把任意楼层映射到这层地面。
    const float HorizontalExtent = Enemy.GetPatrolConfig().ProjectionDistance;
    const float VerticalExtent = HorizontalExtent + Enemy.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f;
    FNavLocation Ground;
    const auto& Props = Enemy.GetNavAgentPropertiesRef();
    if (!Nav->ProjectPointToNavigation(Clue, Ground, FVector(HorizontalExtent, HorizontalExtent, VerticalExtent), &Props) ||
        FVector::DistSquared2D(Clue, Ground.Location) > FMath::Square(HorizontalExtent) ||
        FMath::Abs(Clue.Z-Ground.Location.Z) > VerticalExtent)
        return Fail(TEXT("线索附近没有合适导航面"));
    return FindPath(Enemy, Ground.Location, Anchor, Radius, Projected, Path, true, FailureReason);
}
bool CandyFightPositioning::HasFiringLine(const ACandyFightEnemyCharacter& Enemy, const AActor& Target,
    const FVector& Start, const FVector& OwnerCenter, float Range)
{
    if (Start.ContainsNaN() || OwnerCenter.ContainsNaN()) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightFiringLine), false, &Enemy);
    if (Enemy.Combat->EquippedWeapon) Params.AddIgnoredActor(Enemy.Combat->EquippedWeapon);
    FVector Origin = OwnerCenter; Origin.Z = Start.Z;
    FHitResult Hit;
    auto* World = Enemy.GetWorld();
    if (World->SweepSingleByChannel(Hit, Origin, Start, FQuat::Identity, CandyFightWeapons::TraceChannel,
        FCollisionShape::MakeSphere(1.f), Params) || World->OverlapBlockingTestByChannel(Start, FQuat::Identity,
        CandyFightWeapons::TraceChannel, FCollisionShape::MakeSphere(1.f), Params)) return false;
    const FVector Direction = (Target.GetActorLocation() - Start).GetSafeNormal2D();
    if (Direction.IsNearlyZero()) return false;
    World->LineTraceSingleByChannel(Hit, Start, Start + Direction * Range, CandyFightWeapons::TraceChannel, Params);
    return Hit.bBlockingHit && Hit.GetActor() == &Target;
}
bool CandyFightPositioning::CandidateMuzzle(const ACandyFightEnemyCharacter& Enemy, const FVector& Feet,
    const FVector& Target, FVector& Muzzle, FVector& Center)
{
    const auto* Gun = Enemy.Combat->EquippedWeapon.Get();
    if (!Gun || !Gun->ValidateMuzzle()) return false;
    Center = Feet + (Enemy.GetActorLocation() - Enemy.GetNavAgentLocation());
    const FVector Forward = (Target - Center).GetSafeNormal2D();
    if (Forward.IsNearlyZero()) return false;
    const FTransform Hypothetical(Forward.Rotation(), Center, Enemy.GetActorScale3D());
    const FVector Pivot = Hypothetical.TransformPosition(Enemy.GetActorTransform().InverseTransformPosition(Gun->GetActorLocation()));
    const FVector Local = Gun->GetActorTransform().InverseTransformPosition(Gun->Muzzle->GetComponentLocation()) * Gun->GetActorScale3D();
    const FVector Delta = Target - Pivot;
    const double Distance = Delta.Size2D();
    if (Distance <= Local.Size2D() + 5.) return false;
    const double Yaw = Delta.Rotation().Yaw - FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Local.Y / Distance, -1., 1.)));
    Muzzle = Pivot + FRotator(0,Yaw,0).RotateVector(Local);
    return !Muzzle.ContainsNaN();
}
bool CandyFightPositioning::FindPosition(ACandyFightEnemyCharacter& Enemy, const AActor& Target, const FVector& Anchor,
    float MinDistance, float MaxDistance, const FVector* Previous, FVector& Destination, FNavPathSharedPtr& Path)
{
    Path.Reset();
    const auto& C = *Enemy.EnemyConfig;
    const auto* Weapon = Enemy.Combat->GetCurrentConfig();
    if (!Weapon || MaxDistance <= MinDistance) return false;
    const float Acceptance = PositionAcceptanceRadius(Enemy, MinDistance, MaxDistance);
    const float Margin = FMath::Min(Acceptance + 1.f, (MaxDistance-MinDistance)*.25f);
    const float SamplingInset = FMath::Min(.1f, (MaxDistance-MinDistance)*.01f);
    float Best = MAX_flt, PreviousScore = MAX_flt;
    FVector PreviousPoint; FNavPathSharedPtr PreviousPath;
    const float Preferred = (MinDistance + MaxDistance) * .5f;
    auto Evaluate = [&](const FVector& Candidate, bool bPrevious)
    {
        FVector Point; FNavPathSharedPtr Found;
        if (!FindPath(Enemy, Candidate, Anchor, C.LeashRadius, Point, Found)) return;
        const float Distance = FVector::Dist2D(Point, Target.GetActorLocation());
        // 站位不能压在射击距离边界上，否则导航提前停步时仍不能开火。
        if (Distance < MinDistance + Margin || Distance > MaxDistance - Margin) return;
        // 本函数只在当前位置不满足射击条件时调用，不能再选已经到达的无效站位。
        if (FVector::DistSquared2D(Point, Enemy.GetNavAgentLocation()) <= FMath::Square(Acceptance)) return;
        FVector Muzzle, Center;
        if (!CandidateMuzzle(Enemy, Point, Target.GetActorLocation(), Muzzle, Center)) return;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightPositionOccupancy), false, &Enemy);
        Params.AddIgnoredActor(Enemy.Combat->EquippedWeapon);
        const auto* Capsule = Enemy.GetCapsuleComponent();
        // 略缩胶囊，避免导航面与地面接触被当成站位重叠。
        if (Enemy.GetWorld()->OverlapBlockingTestByChannel(Center + FVector(0,0,2), FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 2), Params)) return;
        if (!HasFiringLine(Enemy, Target, Muzzle, Center, Weapon->Range)) return;
        const float Score = Found->GetLength() + FMath::Abs(Distance - Preferred) * .5f;
        if (bPrevious) { PreviousScore = Score; PreviousPoint = Point; PreviousPath = Found; }
        if (Score < Best) { Best = Score; Destination = Point; Path = Found; }
    };
    if (Previous) Evaluate(*Previous, true);
    const double BaseYaw = (Enemy.GetActorLocation() - Target.GetActorLocation()).Rotation().Yaw;
    for (float Radius : {MinDistance + Margin + SamplingInset, Preferred, MaxDistance - Margin - SamplingInset})
        for (int32 I=0; I<8; ++I)
        {
            FVector P = Target.GetActorLocation() + FRotator(0,BaseYaw+45*I,0).Vector()*Radius;
            P.Z = Enemy.GetNavAgentLocation().Z;
            Evaluate(P, false);
        }
    if (PreviousPath.IsValid() && PreviousScore <= Best + C.PositionRetainTolerance)
        { Destination = PreviousPoint; Path = PreviousPath; }
    return Path.IsValid();
}
