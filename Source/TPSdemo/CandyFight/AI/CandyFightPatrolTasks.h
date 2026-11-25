#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "CandyFightPatrolTasks.generated.h"

UCLASS()
class TPSDEMO_API UCandyFightBTTask_SelectPatrolLocation : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UCandyFightBTTask_SelectPatrolLocation();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

UCLASS()
class TPSDEMO_API UCandyFightBTTask_CompletePatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UCandyFightBTTask_CompletePatrolPoint();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

UCLASS()
class TPSDEMO_API UCandyFightBTTask_HandleMoveFailure : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UCandyFightBTTask_HandleMoveFailure();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

/** Native Move To with CandyFight's blackboard defaults; no shared mutable task state. */
UCLASS()
class TPSDEMO_API UCandyFightBTTask_MoveToPatrol : public UBTTask_MoveTo
{
	GENERATED_BODY()
public:
	UCandyFightBTTask_MoveToPatrol();
};
