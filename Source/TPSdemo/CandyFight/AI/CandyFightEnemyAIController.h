#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CandyFightAutonomyTask.h"
#include "NavigationPath.h"
#include "CandyFightEnemyAIController.generated.h"

class ACandyFightPatrolRoute;

/** 每只敌人自己的巡逻总控：选点、记录进度、处理失败；实际行走交给导航和移动组件。 */
UCLASS()
class TPSDEMO_API ACandyFightEnemyAIController : public AAIController
{
	GENERATED_BODY()
public:
	ACandyFightEnemyAIController();
	// 接管敌人时初始化黑板并启动行为树；失去控制时停止行为和移动。
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	// 定期检查移动是否卡住或超时，不在这里直接改变角色位置。
	virtual void Tick(float DeltaSeconds) override;
	// 以下三个入口由行为树任务调用：选点 → 移动与等待 → 完成；失败则走恢复分支。
    bool SuspendPatrolForWeaponTest();
    void ResumePatrolAfterWeaponTest();
    bool IsPatrolSuspended() const { return bWeaponTestSuspended; }
    bool SelectPatrolLocation();
	void CompletePatrolPoint();
	void HandlePatrolFailure();
	// 调试计数只属于这只敌人，可在运行实例的 Details 中查看。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Patrol|Debug")
	int32 CompletedPoints = 0; // 完成移动且停留结束的次数。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Patrol|Debug")
	int32 FailedAttempts = 0; // 选点或移动失败的总次数。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Patrol|Debug")
	int32 SkippedPoints = 0; // 连续失败后跳过路线点的次数。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Patrol|Debug")
	int32 WatchdogAborts = 0; // 因卡住或超时而主动中止移动的次数。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Patrol|Debug")
	FVector HomeLocation; // 接管时的导航位置，作为随机巡逻范围的中心。
    // 战斗树逐帧调用；巡逻 watchdog 继续以 0.5 秒运行。
    void InitializeAutonomy();
    void UpdateAutonomy(float DeltaSeconds);
    void ReceivePlayerShot(APawn* Player, const FVector& Location);
    bool BeginManualTest();
    void EndManualTest();
    bool CanAutonomousFire() const;
    AActor* GetVisibleCombatTarget() const;
    void BeginReturn(const FString& Reason);
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") ECandyFightEnemyState CombatState = ECandyFightEnemyState::Patrol;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") FVector ReturnAnchor = FVector::ZeroVector;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") FVector LastKnownLocation = FVector::ZeroVector;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") FVector CombatDestination = FVector::ZeroVector;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") FString StopReason;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") bool bTargetVisible = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") TObjectPtr<class UBehaviorTreeComponent> AutonomyBrain;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Autonomy") TObjectPtr<class UBlackboardComponent> AutonomyBlackboard;
private:
    friend class UCandyFightAIEditorLibrary;
    friend class FCandyFightAutonomyTest;
    bool AutonomyEnabled() const;
    void EnterEngagement(APawn* Player, const FVector& Location, bool bSight);
    void SetCombatState(ECandyFightEnemyState State, const FString& Reason);
    void SyncCombatBlackboard();
    void SensePlayer(double Now);
    void UpdatePosition(double Now);
    void UpdateInvestigation(double Now);
    void UpdateSearch(double Now, float DeltaSeconds);
    void UpdateReturn(double Now);
    void FinishReturn();
    void GetCombatDistances(float& Minimum, float& Maximum) const;
    bool RequestCombatMove(const FVector& Destination, const FNavPathSharedPtr& Path, float AcceptanceRadius = -1.f);
    void HaltCombatMove();
    TWeakObjectPtr<APawn> CombatTarget;
    bool bEngaged = false;
    bool bManualFromEngagement = false;
    bool bHasCombatDestination = false;
    bool bLastMoveFailed = false;
    double LastClueTime = 0.;
    double NextSenseTime = 0.;
    double NextPositionTime = 0.;
    double SearchStarted = 0.;
    double LastCombatProgress = 0.;
    double NextReturnRetry = 0.;
    FVector CombatProgressLocation = FVector::ZeroVector;
    int32 SearchPointsUsed = 0;
    int32 ReturnFailures = 0;
    uint64 LastAutonomyFrame = MAX_uint64;
    FNavPathSharedPtr CombatPath;
    friend class FCandyFightEnemyWeaponIntegrationTest;
    bool bWeaponTestSuspended = false;
    bool bSavedOrientToMovement = true;
    bool bPausedBrainForTest = false;
    FAIRequestID PausedMoveId;
    double PatrolPauseStarted = 0.;
    bool bReuseTargetAfterWeaponTest = false;
	void SyncBlackboard(); // 把索引、方向和目标有效性同步给行为树。
	void AdvancePoint(); // 根据循环/往返模式推进索引。
	void ClearTarget(); // 清除本次目的地，但保留巡逻进度。
	bool ProjectReachable(const FVector& Candidate, FVector& Result) const; // 校验可达性，输出导航面上的目标。
	// 配置和路线可以共享，这些运行状态必须每只敌人独立保存。
	int32 PatrolIndex = 0; // 当前路线点的数组索引。
	int32 PatrolDirection = 1; // 往返方向：1 正向，-1 反向。
	int32 PointFailures = 0; // 当前点连续失败次数，区别于上面的失败总数。
	bool bHadRoute = false; // 曾指定过路线：路线失效后仍重试，不自动变成随机巡逻。
	bool bHasTarget = false; // 是否已选好本次目的地。
	TWeakObjectPtr<ACandyFightPatrolRoute> ActiveRoute; // 弱引用，路线销毁后自动失效。
	FVector ProgressLocation; // 上一次确认取得移动进展的位置。
	double MoveStarted = 0; // 本次选点成功时的游戏时间。
	double LastProgress = 0; // 上次取得明显进展的游戏时间。
	double LastWarning = -30; // 限制重复日志的输出频率。
};
