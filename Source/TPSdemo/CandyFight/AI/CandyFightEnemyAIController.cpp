#include "CandyFightEnemyAIController.h"
#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFightPatrolRoute.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "CandyFightEnemyWeaponComponent.h"
#include "CandyFightPatrolBrainComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCandyFightPatrol, Log, All);

ACandyFightEnemyAIController::ACandyFightEnemyAIController()
{
    BrainComponent = CreateDefaultSubobject<UCandyFightPatrolBrainComponent>(TEXT("PatrolBrain"));
	PrimaryActorTick.bCanEverTick = true;
	// 巡逻监测不需要每帧执行；实际移动仍由 CharacterMovement 持续更新。
	PrimaryActorTick.TickInterval = 0.5f;
}

void ACandyFightEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ACandyFightEnemyCharacter* Enemy = Cast<ACandyFightEnemyCharacter>(InPawn);
	if (!Enemy) return;
	// 接管新的敌人时，从初始进度开始，并记住随机巡逻的中心和指定路线。
	PatrolIndex = 0; PatrolDirection = 1; PointFailures = 0;
	CompletedPoints = FailedAttempts = SkippedPoints = WatchdogAborts = 0;
	bHasTarget = false;
	HomeLocation = Enemy->GetNavAgentLocation();
	ActiveRoute = Enemy->PatrolRoute;
	bHadRoute = Enemy->PatrolRoute != nullptr;
	if (Enemy->EnemyConfig && !Enemy->EnemyConfig->IsValidConfiguration())
		UE_LOG(LogCandyFightPatrol, Warning, TEXT("%s: invalid config, using safe defaults"), *Enemy->GetName());
	// 黑板资产定义字段；UseBlackboard 为这只控制器准备独立的黑板数据。
	UBlackboardComponent* BB = nullptr;
	if (!Enemy->PatrolBehaviorTree || !Enemy->PatrolBehaviorTree->BlackboardAsset ||
		!UseBlackboard(Enemy->PatrolBehaviorTree->BlackboardAsset, BB))
	{
		UE_LOG(LogCandyFightPatrol, Warning, TEXT("%s: assign BT_CandyFightEnemy and its blackboard"), *Enemy->GetName());
		return;
	}
	// GetPatrolConfig 会在配置缺失或非法时返回安全默认值。
	const UCandyFightEnemyConfig& Config = Enemy->GetPatrolConfig();
	BB->SetValueAsVector(TEXT("HomeLocation"), HomeLocation);
	BB->SetValueAsFloat(TEXT("AcceptanceRadius"), Config.AcceptanceRadius);
	BB->SetValueAsFloat(TEXT("RetryDelay"), Config.RetryDelay);
	SyncBlackboard();
	// 启动后由行为树安排选点、移动、等待，本函数不直接发起行走。
	if (!RunBehaviorTree(Enemy->PatrolBehaviorTree))
		UE_LOG(LogCandyFightPatrol, Error, TEXT("%s: behavior tree failed to start"), *Enemy->GetName());
    InitializeAutonomy();
}

void ACandyFightEnemyAIController::OnUnPossess()
{
    if (AutonomyBrain) AutonomyBrain->StopTree(EBTStopMode::Forced);
    bEngaged = false; CombatTarget.Reset(); bTargetVisible = false; CombatPath.Reset();
    CombatState = ECandyFightEnemyState::Patrol;
    if (auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn()))
    {
        Enemy->WeaponDriver->StopTest(false);
        Enemy->Combat->ShutdownCombat();
        if (bWeaponTestSuspended) Enemy->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrientToMovement;
    }
    if (bPausedBrainForTest) CastChecked<UCandyFightPatrolBrainComponent>(BrainComponent)->SetWeaponTestPaused(false);
    bPausedBrainForTest = false;
    bWeaponTestSuspended = false;
    // 先停行为和路径跟随，避免脱离角色后继续执行旧任务。
	if (BrainComponent) BrainComponent->StopLogic(TEXT("Enemy unpossessed"));
	StopMovement();
	ClearTarget();
	Super::OnUnPossess();
}

void ACandyFightEnemyAIController::SyncBlackboard()
{
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsInt(TEXT("PatrolIndex"), PatrolIndex);
		BB->SetValueAsInt(TEXT("PatrolDirection"), PatrolDirection);
		BB->SetValueAsBool(TEXT("HasPatrolLocation"), bHasTarget);
	}
}

void ACandyFightEnemyAIController::ClearTarget()
{
	bHasTarget = false;
	if (GetBlackboardComponent()) GetBlackboardComponent()->ClearValue(TEXT("PatrolLocation"));
	SyncBlackboard();
}

bool ACandyFightEnemyAIController::ProjectReachable(const FVector& Candidate, FVector& Result) const
{
	const ACandyFightEnemyCharacter* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn());
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Enemy || !Nav || Candidate.ContainsNaN()) return false;
	const float Extent = Enemy->GetPatrolConfig().ProjectionDistance;
	FNavLocation Projected;
	const FNavAgentProperties& Agent = Enemy->GetNavAgentPropertiesRef();
	// 路线点未必正好在导航面上：允许小范围投影，但不能偏离原点太远。
	if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(Extent), &Agent) ||
		FVector::Dist(Candidate, Projected.Location) > Extent) return false;
	// 已经站在目的地时直接视为可达；否则引擎的单点路径可能被误判为无效。
	const FVector Start = Enemy->GetNavAgentLocation();
	if (FVector::DistSquared2D(Start, Projected.Location) < 1.0 && FMath::Abs(Start.Z - Projected.Location.Z) < 20.0)
	{
		Result = Projected.Location;
		return true;
	}
	const ANavigationData* Data = Nav->GetNavDataForProps(Agent, Start);
	if (!Data) return false;
	FPathFindingQuery Query(GetPawn(), *Data, Start, Projected.Location);
	// 必须能完整到达目标，不能接受只走到障碍前就结束的部分路径。
	Query.SetAllowPartialPaths(false);
	const FPathFindingResult Path = Nav->FindPathSync(Agent, Query);
	if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial()) return false;
	Result = Projected.Location;
	return true;
}

bool ACandyFightEnemyAIController::SelectPatrolLocation()
{
	ACandyFightEnemyCharacter* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn());
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!Enemy || !BB) return false;
	const UCandyFightEnemyConfig& Config = Enemy->GetPatrolConfig();
	// 运行中换成新的有效路线，下次选点时从头开始；空/已删除路线不会切换为随机模式。
	if (IsValid(Enemy->PatrolRoute) && Enemy->PatrolRoute != ActiveRoute.Get())
	{
		ActiveRoute = Enemy->PatrolRoute; bHadRoute = true;
		PatrolIndex = 0; PatrolDirection = 1; PointFailures = 0;
	}
    const bool bReuse = bReuseTargetAfterWeaponTest && bHasTarget;
    const FVector PreviousTarget = BB->GetValueAsVector(TEXT("PatrolLocation"));
    bReuseTargetAfterWeaponTest = false;
    ClearTarget();
    FVector Target = FVector::ZeroVector;
	bool bFound = false;
    if (bReuse) bFound = ProjectReachable(PreviousTarget, Target);
    else if (bHadRoute)
	{
		// 固定路线每次只检查当前点，失败交给恢复流程，避免一帧遍历大量坏点。
		if (const ACandyFightPatrolRoute* Route = ActiveRoute.Get(); Route && !Route->Points.IsEmpty())
		{
			PatrolIndex = FMath::Clamp(PatrolIndex, 0, Route->Points.Num() - 1);
			bFound = ProjectReachable(Route->GetWorldPoint(PatrolIndex), Target);
		}
	}
	else if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		// 未指定路线：围绕出生位置找随机点，最多尝试 8 次，并排除离自己过近的点。
		ANavigationData* Data = Nav->GetNavDataForProps(Enemy->GetNavAgentPropertiesRef(), HomeLocation);
		for (int32 Attempt = 0; Attempt < 8 && !bFound; ++Attempt)
		{
			FNavLocation Random;
			if (Nav->GetRandomReachablePointInRadius(HomeLocation, Config.RandomRadius, Random, Data) &&
				FVector::Dist2D(HomeLocation, Random.Location) <= Config.RandomRadius &&
				FVector::Dist2D(Enemy->GetActorLocation(), Random.Location) > Config.AcceptanceRadius * 1.5f)
				bFound = ProjectReachable(Random.Location, Target);
			if (bFound && FVector::Dist2D(HomeLocation, Target) > Config.RandomRadius) bFound = false;
		}
	}
	// 选点失败时在这里记账；返回 false 后，行为树进入 RetryDelay 等待。
	if (!bFound) { HandlePatrolFailure(); return false; }
	// 这里只把目的地交给黑板；后续 Move To 任务才负责请求路径跟随。
	bHasTarget = true;
	BB->SetValueAsVector(TEXT("PatrolLocation"), Target);
	BB->SetValueAsFloat(TEXT("WaitDuration"), FMath::FRandRange(Config.WaitMin, Config.WaitMax));
	ProgressLocation = Enemy->GetActorLocation();
	MoveStarted = LastProgress = GetWorld()->GetTimeSeconds();
	SyncBlackboard();
	return true;
}

void ACandyFightEnemyAIController::AdvancePoint()
{
	if (const ACandyFightPatrolRoute* Route = ActiveRoute.Get())
		ACandyFightPatrolRoute::AdvanceCursor(Route->Points.Num(), Route->bPingPong, PatrolIndex, PatrolDirection);
}

void ACandyFightEnemyAIController::CompletePatrolPoint()
{
	// 行为树在“移动成功 + 停留结束”后调用；随机模式没有路线索引需要推进。
	++CompletedPoints;
	PointFailures = 0;
	AdvancePoint();
	ClearTarget();
}

void ACandyFightEnemyAIController::HandlePatrolFailure()
{
    if (bWeaponTestSuspended || bReuseTargetAfterWeaponTest) return;
	const ACandyFightEnemyCharacter* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn());
	if (!Enemy) return;
	++FailedAttempts;
	// 达到连续失败上限时跳到下一点；不是永久删除，本轮绕回后仍会重试。
	if (ACandyFightPatrolRoute::RegisterFailure(Enemy->GetPatrolConfig().MaxPointFailures, PointFailures))
	{
		if (ActiveRoute.IsValid() && !ActiveRoute->Points.IsEmpty()) { AdvancePoint(); ++SkippedPoints; }
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// 持续失败时最多每 15 秒打印一次提示，避免刷屏。
	if (Now - LastWarning >= 15.0)
	{
		UE_LOG(LogCandyFightPatrol, Display, TEXT("%s: patrol retry (route=%s, index=%d, failures=%d). Check NavMesh / obstruction / route points."),
			*Enemy->GetName(), *GetNameSafe(ActiveRoute.Get()), PatrolIndex, FailedAttempts);
		LastWarning = Now;
	}
	ClearTarget();
}

void ACandyFightEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ACandyFightEnemyCharacter* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn());
	// 到点停留或没有目标时不做卡住检测。
	if (bWeaponTestSuspended || !Enemy || !bHasTarget || GetMoveStatus() != EPathFollowingStatus::Moving) return;
	const double Now = GetWorld()->GetTimeSeconds();
	// 相对上次记录位置累计移动至少 20 cm 才算新进展，并非每 0.5 秒都必须走满 20 cm。
	if (FVector::DistSquared2D(ProgressLocation, Enemy->GetActorLocation()) >= FMath::Square(20.0))
	{
		ProgressLocation = Enemy->GetActorLocation(); LastProgress = Now;
	}
	const UCandyFightEnemyConfig& Config = Enemy->GetPatrolConfig();
	// 两道保护：太久没有进展，或者本次移动总耗时过长（例如一直绕圈）。
	if (Now - LastProgress > Config.StuckTimeout || Now - MoveStarted > Config.MoveTimeout)
	{
		++WatchdogAborts;
		// 停止移动会让 Move To 收到失败，行为树随后调用失败处理并等待重试。
		StopMovement();
	}
}

// 暂停现有任务而不是 StopLogic，保留 Wait 剩余时间和路线进度。
bool ACandyFightEnemyAIController::SuspendPatrolForWeaponTest()
{
    if (bWeaponTestSuspended) return true;
    auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn());
    if (!Enemy) return false;
    bWeaponTestSuspended = true;
    PatrolPauseStarted = GetWorld()->GetTimeSeconds();
    bPausedBrainForTest = BrainComponent && BrainComponent->IsRunning();
    if (bPausedBrainForTest) CastChecked<UCandyFightPatrolBrainComponent>(BrainComponent)->SetWeaponTestPaused(true);
    PausedMoveId = FAIRequestID::InvalidRequest;
    if (GetMoveStatus() == EPathFollowingStatus::Moving || GetMoveStatus() == EPathFollowingStatus::Waiting)
    {
        PausedMoveId = GetCurrentMoveRequestID();
        PauseMove(PausedMoveId);
    }
    bSavedOrientToMovement = Enemy->GetCharacterMovement()->bOrientRotationToMovement;
    Enemy->GetCharacterMovement()->bOrientRotationToMovement = false;
    Enemy->GetCharacterMovement()->StopMovementKeepPathing();
    return true;
}
void ACandyFightEnemyAIController::ResumePatrolAfterWeaponTest()
{
    if (!bWeaponTestSuspended) return;
    const double Duration = GetWorld()->GetTimeSeconds() - PatrolPauseStarted;
    MoveStarted += Duration; LastProgress += Duration;
    if (auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetPawn()))
        Enemy->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrientToMovement;
    // 路径恢复失败时让原 MoveTo 重试同一点，测试暂停不记为失败。
    bool bNeedsRestart = false;
    if (PausedMoveId.IsValid())
        bNeedsRestart = !ResumeMove(PausedMoveId) || GetMoveStatus() != EPathFollowingStatus::Moving;
    bReuseTargetAfterWeaponTest = bNeedsRestart && bHasTarget;
    if (bPausedBrainForTest) CastChecked<UCandyFightPatrolBrainComponent>(BrainComponent)->SetWeaponTestPaused(false);
    if (bNeedsRestart && bPausedBrainForTest) BrainComponent->RestartLogic();
    bWeaponTestSuspended = false;
    PausedMoveId = FAIRequestID::InvalidRequest;
    bPausedBrainForTest = false;
}
