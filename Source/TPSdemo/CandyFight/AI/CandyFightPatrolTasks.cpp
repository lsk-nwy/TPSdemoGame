#include "CandyFightPatrolTasks.h"
#include "CandyFightEnemyAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UCandyFightBTTask_SelectPatrolLocation::UCandyFightBTTask_SelectPatrolLocation() { NodeName = TEXT("Select patrol location"); }
UCandyFightBTTask_CompletePatrolPoint::UCandyFightBTTask_CompletePatrolPoint() { NodeName = TEXT("Complete patrol point"); }
UCandyFightBTTask_HandleMoveFailure::UCandyFightBTTask_HandleMoveFailure() { NodeName = TEXT("Handle move failure"); }

EBTNodeResult::Type UCandyFightBTTask_SelectPatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ACandyFightEnemyAIController* AI = Cast<ACandyFightEnemyAIController>(OwnerComp.GetAIOwner());
	return AI && AI->SelectPatrolLocation() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

EBTNodeResult::Type UCandyFightBTTask_CompletePatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (ACandyFightEnemyAIController* AI = Cast<ACandyFightEnemyAIController>(OwnerComp.GetAIOwner()))
	{
		AI->CompletePatrolPoint(); return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UCandyFightBTTask_HandleMoveFailure::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (ACandyFightEnemyAIController* AI = Cast<ACandyFightEnemyAIController>(OwnerComp.GetAIOwner())) AI->HandlePatrolFailure();
	// Fail upward to the root fallback Wait (RetryDelay).
	return EBTNodeResult::Failed;
}

UCandyFightBTTask_MoveToPatrol::UCandyFightBTTask_MoveToPatrol()
{
	NodeName = TEXT("Move To Patrol (native path following)");
	BlackboardKey.SelectedKeyName = TEXT("PatrolLocation");
	AcceptableRadius.SetKey(TEXT("AcceptanceRadius"));
	bAllowPartialPath = FValueOrBBKey_Bool(false);
	bReachTestIncludesAgentRadius = FValueOrBBKey_Bool(false);
	bReachTestIncludesGoalRadius = FValueOrBBKey_Bool(false);
	bProjectGoalLocation = FValueOrBBKey_Bool(false);
}
