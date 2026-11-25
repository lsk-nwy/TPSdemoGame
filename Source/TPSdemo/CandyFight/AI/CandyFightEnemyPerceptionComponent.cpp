#include "CandyFightEnemyPerceptionComponent.h"
#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFightEnemyAIController.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

bool UCandyFightEnemyPerceptionComponent::InSightCone(const FVector& Origin, const FVector& Forward, const FVector& Target, float Distance, float TotalAngle)
{
    if (Origin.ContainsNaN() || Target.ContainsNaN() || Forward.ContainsNaN()) return false;
    const FVector Delta = Target - Origin;
    return Delta.SizeSquared2D() <= FMath::Square(Distance) &&
        (Delta.IsNearlyZero() || FVector::DotProduct(Forward.GetSafeNormal2D(), Delta.GetSafeNormal2D()) + 1.e-6 >= FMath::Cos(FMath::DegreesToRadians(TotalAngle * .5f)));
}
bool UCandyFightEnemyPerceptionComponent::CanSee(const AActor* Target) const
{
    const auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner());
    if (!Enemy || !IsValid(Target) || !Enemy->EnemyConfig) return false;
    const auto& C = *Enemy->EnemyConfig;
    const FVector Eye = Enemy->GetActorLocation() + FVector(0,0,C.SightHeight);
    if (!InSightCone(Eye, Enemy->GetActorForwardVector(), Target->GetActorLocation(), C.SightDistance, C.SightAngleDegrees)) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightSight), false, Enemy);
    // 敌人不遮挡同伴的视野，但会遮挡实际射线；两种查询有意分开。
    for (TActorIterator<ACandyFightEnemyCharacter> It(GetWorld()); It; ++It)
    {
        Params.AddIgnoredActor(*It);
        if (It->Combat->EquippedWeapon) Params.AddIgnoredActor(It->Combat->EquippedWeapon);
    }
    FHitResult Hit;
    GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation(), ECC_Visibility, Params);
    return !Hit.bBlockingHit || Hit.GetActor() == Target;
}
void UCandyFightEnemyPerceptionComponent::ReportPlayerShot(APawn* Player, const FVector& MuzzleLocation)
{
    const auto* PC = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController() || Player != UGameplayStatics::GetPlayerPawn(Player, 0) || MuzzleLocation.ContainsNaN()) return;
    for (TActorIterator<ACandyFightEnemyCharacter> It(Player->GetWorld()); It; ++It)
        if (auto* AI = Cast<ACandyFightEnemyAIController>(It->GetController())) AI->ReceivePlayerShot(Player, MuzzleLocation);
}
