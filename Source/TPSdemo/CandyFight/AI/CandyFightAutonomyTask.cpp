#include "CandyFightAutonomyTask.h"
#include "CandyFightEnemyAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
UCandyFightAutonomyTask::UCandyFightAutonomyTask() { bNotifyTick = true; NodeName = TEXT("CandyFight 阶段执行"); }
EBTNodeResult::Type UCandyFightAutonomyTask::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    auto* AI = Cast<ACandyFightEnemyAIController>(OwnerComp.GetAIOwner());
    return AI && AI->CombatState == Phase ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}
void UCandyFightAutonomyTask::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    auto* AI = Cast<ACandyFightEnemyAIController>(OwnerComp.GetAIOwner());
    if (!AI) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }
    AI->UpdateAutonomy(DeltaSeconds);
    if (AI->CombatState != Phase) FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
