#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CandyFightEnemyPerceptionComponent.generated.h"

/** 只判断能否看到、能否听到。追踪记忆和行为决策属于 Controller。 */
UCLASS(ClassGroup=(CandyFight), meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightEnemyPerceptionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    bool CanSee(const AActor* Target) const;
    static bool InSightCone(const FVector& Origin, const FVector& Forward, const FVector& Target, float Distance, float TotalAngle);
    // 由成功发射入口调用一次，与声音是否播放无关。只接受本地玩家。
    static void ReportPlayerShot(APawn* Player, const FVector& MuzzleLocation);
};
