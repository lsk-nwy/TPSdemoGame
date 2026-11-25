#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CandyFightAutonomyTask.generated.h"

UENUM(BlueprintType)
enum class ECandyFightEnemyState : uint8 { Patrol, Investigate, Position, Fire, Search, Return, ManualTest };

/** 每个分支驱动对应阶段，结束时重新选择分支；节点无共享运行状态。 */
UCLASS()
class TPSDEMO_API UCandyFightAutonomyTask : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UCandyFightAutonomyTask();
    UPROPERTY(EditAnywhere, Category="Autonomy") ECandyFightEnemyState Phase = ECandyFightEnemyState::Patrol;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
