#include "CandyFightEnemyAIController.h"
#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFightEnemyPerceptionComponent.h"
#include "CandyFightEnemyWeaponComponent.h"
#include "CandyFightCombatPositioning.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarCandyFightAIDebug(TEXT("CandyFight.AIDebug"), 0,
    TEXT("1: sight, hearing, leash, clue, destination, state and stop reason"), ECVF_Cheat);

bool ACandyFightEnemyAIController::AutonomyEnabled() const
{
    const auto* E = Cast<ACandyFightEnemyCharacter>(GetPawn());
    return E && E->EnemyConfig && E->EnemyConfig->AutonomyTree && E->EnemyConfig->bEnableAutonomousCombat &&
        E->EnemyConfig->IsValidAutonomy() && E->EnemyConfig->IsValidWeaponDriver();
}
void ACandyFightEnemyAIController::InitializeAutonomy()
{
    if (!AutonomyEnabled()) return;
    const auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    auto* Tree = E->EnemyConfig->AutonomyTree.Get();
    if (!Tree || !Tree->BlackboardAsset) return;
    if (!AutonomyBlackboard)
    {
        AutonomyBlackboard = NewObject<UBlackboardComponent>(this, TEXT("CombatBlackboard"));
        AutonomyBlackboard->RegisterComponent();
        AutonomyBrain = NewObject<UBehaviorTreeComponent>(this, TEXT("CombatBrain"));
        AutonomyBrain->RegisterComponent();
    }
    // UE 默认寻找第一个 Brain/Blackboard；显式配对，避免覆盖原巡逻树的黑板。
    BrainComponent->CacheBlackboardComponent(GetBlackboardComponent());
    GetBlackboardComponent()->CacheBrainComponent(*BrainComponent);
    AutonomyBlackboard->InitializeBlackboard(*Tree->BlackboardAsset);
    AutonomyBrain->CacheBlackboardComponent(AutonomyBlackboard);
    AutonomyBlackboard->CacheBrainComponent(*AutonomyBrain);
    CombatState = ECandyFightEnemyState::Patrol;
    LastAutonomyFrame = MAX_uint64;
    NextSenseTime = GetWorld()->GetTimeSeconds() + (GetUniqueID()%10)*.01;
    SyncCombatBlackboard();
    AutonomyBrain->StartTree(*Tree);
}
void ACandyFightEnemyAIController::SyncCombatBlackboard()
{
    if (!AutonomyBlackboard) return;
    AutonomyBlackboard->SetValueAsInt(TEXT("CombatState"), static_cast<int32>(CombatState));
    AutonomyBlackboard->SetValueAsString(TEXT("StateName"), StaticEnum<ECandyFightEnemyState>()->GetNameStringByValue(static_cast<int64>(CombatState)));
    AutonomyBlackboard->SetValueAsBool(TEXT("TargetVisible"), bTargetVisible);
    AutonomyBlackboard->SetValueAsVector(TEXT("LastKnownLocation"), LastKnownLocation);
    AutonomyBlackboard->SetValueAsVector(TEXT("ReturnAnchor"), ReturnAnchor);
    AutonomyBlackboard->SetValueAsVector(TEXT("Destination"), CombatDestination);
    AutonomyBlackboard->SetValueAsString(TEXT("StopReason"), StopReason);
}
void ACandyFightEnemyAIController::SetCombatState(ECandyFightEnemyState State, const FString& Reason)
{
    if (State == CombatState) return;
    if (auto* E = Cast<ACandyFightEnemyCharacter>(GetPawn()))
    {
        E->WeaponDriver->InterruptAutonomousBurst();
        E->GetCharacterMovement()->bOrientRotationToMovement = State != ECandyFightEnemyState::Fire && State != ECandyFightEnemyState::ManualTest;
    }
    CombatState = State; StopReason = Reason;
    SyncCombatBlackboard();
}
void ACandyFightEnemyAIController::HaltCombatMove()
{
    StopMovement();
    if (auto* E = Cast<ACandyFightEnemyCharacter>(GetPawn())) E->GetCharacterMovement()->StopMovementKeepPathing();
    bHasCombatDestination = false; CombatPath.Reset();
}
void ACandyFightEnemyAIController::EnterEngagement(APawn* Player, const FVector& Location, bool bSight)
{
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    const bool bReacquiring = CombatState == ECandyFightEnemyState::Return;
    if (!bEngaged)
    {
        ReturnAnchor = E->GetNavAgentLocation();
        SuspendPatrolForWeaponTest();
        // 原移动请求会失效；原 Wait 的节点内存仍保留在暂停的巡逻树中。
        HaltCombatMove();
        bEngaged = true;
        CombatProgressLocation = E->GetActorLocation();
        LastCombatProgress = GetWorld()->GetTimeSeconds();
    }
    if (bReacquiring)
    {
        // 归途中重新响应线索，仍属于同一次交战；保留归位点及暂停的巡逻进度。
        // 重新开始移动进展计时，避免旧的卡住超时让本帧立即再次归位。
        CombatProgressLocation = E->GetActorLocation();
        LastCombatProgress = GetWorld()->GetTimeSeconds();
        bLastMoveFailed = false;
    }
    CombatTarget = Player; LastKnownLocation = Location;
    LastClueTime = GetWorld()->GetTimeSeconds(); bTargetVisible = bSight;
    if (bReacquiring || CombatState == ECandyFightEnemyState::Patrol || CombatState == ECandyFightEnemyState::Search ||
        (bSight && CombatState == ECandyFightEnemyState::Investigate))
    {
        HaltCombatMove();
        SetCombatState(bSight ? ECandyFightEnemyState::Position : ECandyFightEnemyState::Investigate, bSight ? TEXT("发现玩家") : TEXT("听到玩家枪声"));
        NextPositionTime = LastClueTime + (GetUniqueID()%5)*.02;
    }
    E->GetCharacterMovement()->MaxWalkSpeed = E->EnemyConfig->CombatWalkSpeed;
}
void ACandyFightEnemyAIController::ReceivePlayerShot(APawn* Player, const FVector& Location)
{
    if (!AutonomyEnabled() || !AutonomyBrain || !AutonomyBrain->IsRunning() || !IsValid(Player) || Location.ContainsNaN() ||
        CombatState == ECandyFightEnemyState::ManualTest ||
        UGameplayStatics::IsGamePaused(this)) return;
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    const auto& C = *E->EnemyConfig;
    if (FVector::DistSquared(E->GetActorLocation(), Location) > FMath::Square(C.HearingDistance) ||
        !CandyFightPositioning::InsideLeash(Location, bEngaged ? ReturnAnchor : E->GetNavAgentLocation(), C.LeashRadius)) return;
    if (bEngaged && !CandyFightPositioning::InsideLeash(E->GetNavAgentLocation(), ReturnAnchor, C.LeashRadius)) return;
    if (bTargetVisible && E->Perception->CanSee(Player)) return; // 实际可见线索优先。
    EnterEngagement(Player, Location, false);
    if (CombatState != ECandyFightEnemyState::Investigate)
    {
        HaltCombatMove(); SetCombatState(ECandyFightEnemyState::Investigate, TEXT("枪声更新调查点"));
    }
    NextPositionTime = FMath::Min(NextPositionTime, GetWorld()->GetTimeSeconds());
    SyncCombatBlackboard();
}
void ACandyFightEnemyAIController::SensePlayer(double Now)
{
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    const bool bSeen = IsValid(Player) && E->Perception->CanSee(Player);
    if (bSeen)
    {
        const FVector Location = Player->GetActorLocation();
        if (bEngaged && (!CandyFightPositioning::InsideLeash(Location, ReturnAnchor, E->EnemyConfig->LeashRadius) ||
            !CandyFightPositioning::InsideLeash(E->GetNavAgentLocation(), ReturnAnchor, E->EnemyConfig->LeashRadius)))
            { BeginReturn(TEXT("玩家离开追踪范围")); return; }
        EnterEngagement(Player, Location, true);
    }
    else
    {
        bTargetVisible = false;
        if (CombatState == ECandyFightEnemyState::Position || CombatState == ECandyFightEnemyState::Fire)
        {
            HaltCombatMove(); SetCombatState(ECandyFightEnemyState::Investigate, TEXT("失去视野，调查最后已知位置"));
            NextPositionTime = Now;
        }
    }
}
void ACandyFightEnemyAIController::GetCombatDistances(float& Minimum, float& Maximum) const
{
    const auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    const auto& C = *E->EnemyConfig;
    const auto* W = E->Combat->GetCurrentConfig();
    const bool bShotgun = W && W->FireMode == ECandyFightFireMode::Shotgun;
    Minimum = bShotgun ? C.ShotgunMinDistance : C.RifleMinDistance;
    Maximum = bShotgun ? C.ShotgunMaxDistance : C.RifleMaxDistance;
    if (W) Maximum = FMath::Min(Maximum, W->Range);
}
AActor* ACandyFightEnemyAIController::GetVisibleCombatTarget() const
{
    const auto* E = Cast<ACandyFightEnemyCharacter>(GetPawn());
    return bTargetVisible && E && CombatTarget.IsValid() && E->Perception->CanSee(CombatTarget.Get()) ? CombatTarget.Get() : nullptr;
}
bool ACandyFightEnemyAIController::CanAutonomousFire() const
{
    if (!AutonomyEnabled() || CombatState != ECandyFightEnemyState::Fire) return false;
    const auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    const auto* Target = GetVisibleCombatTarget();
    const auto* W = E->Combat->GetCurrentConfig();
    const auto* Gun = E->Combat->EquippedWeapon.Get();
    if (!Target || !W || !Gun || !Gun->ValidateMuzzle() || E->GetVelocity().Size2D() > E->EnemyConfig->MaxFireSpeed ||
        !CandyFightPositioning::InsideLeash(Target->GetActorLocation(), ReturnAnchor, E->EnemyConfig->LeashRadius) ||
        !CandyFightPositioning::InsideLeash(E->GetNavAgentLocation(), ReturnAnchor, E->EnemyConfig->LeashRadius)) return false;
    float Min, Max; GetCombatDistances(Min, Max);
    const float Distance = FVector::Dist2D(E->GetActorLocation(), Target->GetActorLocation());
    if (Distance < FMath::Max(0.f, Min-E->EnemyConfig->DistanceHysteresis) ||
        Distance > FMath::Min(W->Range, Max+E->EnemyConfig->DistanceHysteresis)) return false;
    return CandyFightPositioning::HasFiringLine(*E, *Target, Gun->Muzzle->GetComponentLocation(), E->GetActorLocation(), W->Range);
}
bool ACandyFightEnemyAIController::RequestCombatMove(const FVector& Destination, const FNavPathSharedPtr& Path, float AcceptanceRadius)
{
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    const float Radius = AcceptanceRadius >= 0.f ? AcceptanceRadius : E->EnemyConfig->DestinationTolerance;
    if (bHasCombatDestination && FVector::DistSquared2D(Destination, CombatDestination) < FMath::Square(Radius) &&
        GetMoveStatus() == EPathFollowingStatus::Moving && CombatPath.IsValid() && CombatPath->IsValid()) return true;
    FAIMoveRequest Request(Destination);
    Request.SetAcceptanceRadius(Radius);
    Request.SetReachTestIncludesAgentRadius(false); Request.SetReachTestIncludesGoalRadius(false);
    Request.SetAllowPartialPath(false); Request.SetProjectGoalLocation(false);
    const FAIRequestID ID = RequestMove(Request, Path);
    bHasCombatDestination = ID.IsValid(); bLastMoveFailed = !bHasCombatDestination;
    if (bHasCombatDestination) { CombatDestination = Destination; CombatPath = Path; }
    return bHasCombatDestination;
}
void ACandyFightEnemyAIController::UpdatePosition(double Now)
{
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    AActor* Target = GetVisibleCombatTarget();
    if (!Target)
    {
        bTargetVisible = false; HaltCombatMove();
        SetCombatState(ECandyFightEnemyState::Investigate, TEXT("视野被挡")); NextPositionTime = Now; return;
    }
    auto* W = E->Combat->GetCurrentConfig(); auto* Gun = E->Combat->EquippedWeapon.Get();
    if (!W || !Gun || !Gun->ValidateMuzzle()) { BeginReturn(TEXT("武器配置或枪口不可用")); return; }
    const FVector TargetLocation = Target->GetActorLocation();
    E->SetActorRotation(FRotator(0,(TargetLocation-E->GetActorLocation()).Rotation().Yaw,0));
    E->GetCharacterMovement()->bOrientRotationToMovement = false;
    Gun->AimAt(TargetLocation, E->GetActorForwardVector());
    float Min, Max; GetCombatDistances(Min, Max);
    const float Distance = FVector::Dist2D(E->GetActorLocation(), TargetLocation);
    const float Buffer = CombatState == ECandyFightEnemyState::Fire ? E->EnemyConfig->DistanceHysteresis : 0.f;
    const bool bGood = Distance >= FMath::Max(0.f,Min-Buffer) && Distance <= FMath::Min(W->Range,Max+Buffer) &&
        CandyFightPositioning::HasFiringLine(*E,*Target,Gun->Muzzle->GetComponentLocation(),E->GetActorLocation(),W->Range);
    if (bGood)
    {
        HaltCombatMove(); SetCombatState(ECandyFightEnemyState::Fire, TEXT("距离及枪口通路有效，停步射击"));
        LastCombatProgress = Now; return;
    }
    SetCombatState(ECandyFightEnemyState::Position, TEXT("距离或枪口通路不满足，重新寻位"));
    if (Now < NextPositionTime) return;
    NextPositionTime = Now + E->EnemyConfig->PositionInterval;
    FVector Destination; FNavPathSharedPtr Path;
    if (CandyFightPositioning::FindPosition(*E,*Target,ReturnAnchor,Min,Max,bHasCombatDestination?&CombatDestination:nullptr,Destination,Path))
    {
        const TCHAR* Purpose = Distance < Min ? TEXT("后退至射击距离") : Distance > Max ? TEXT("接近至射击距离") : TEXT("绕开枪口遮挡");
        StopReason = RequestCombatMove(Destination,Path,CandyFightPositioning::PositionAcceptanceRadius(*E,Min,Max))
            ? FString(Purpose) : TEXT("射击站位移动请求失败");
    }
    else
    {
        // 无射击位置时只接近已确认线索，不能透墙跟随隐藏对象。
        if (CandyFightPositioning::FindCluePath(*E,LastKnownLocation,ReturnAnchor,E->EnemyConfig->LeashRadius,Destination,Path))
            StopReason = RequestCombatMove(Destination,Path) ? TEXT("暂无射击站位，接近最后线索") : TEXT("线索移动请求失败");
        else { HaltCombatMove(); StopReason = TEXT("没有可达射击位置或线索路径"); }
    }
}
void ACandyFightEnemyAIController::UpdateInvestigation(double Now)
{
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    const auto& C = *E->EnemyConfig;
    const bool bReachedProjectedClue = bHasCombatDestination &&
        FVector::Dist2D(CombatDestination,LastKnownLocation) <= E->GetPatrolConfig().ProjectionDistance &&
        FVector::Dist2D(E->GetNavAgentLocation(),CombatDestination) <= C.DestinationTolerance;
    if (FVector::Dist2D(E->GetNavAgentLocation(),LastKnownLocation) <= C.DestinationTolerance || bReachedProjectedClue)
    {
        HaltCombatMove(); SetCombatState(ECandyFightEnemyState::Search,TEXT("到达最后线索，移动搜索"));
        SearchStarted = Now; SearchPointsUsed = 0; E->GetCharacterMovement()->MaxWalkSpeed = C.WalkSpeed; return;
    }
    if (Now < NextPositionTime) return;
    NextPositionTime = Now + C.PositionInterval;
    FVector Destination; FNavPathSharedPtr Path;
    FString FailureReason;
    if (CandyFightPositioning::FindCluePath(*E,LastKnownLocation,ReturnAnchor,C.LeashRadius,Destination,Path,&FailureReason))
        StopReason = RequestCombatMove(Destination,Path) ? TEXT("前往最后线索位置") : TEXT("调查移动请求失败");
    else { HaltCombatMove(); StopReason = TEXT("调查点暂不可达：") + FailureReason; }
}
void ACandyFightEnemyAIController::UpdateSearch(double Now, float DeltaSeconds)
{
    auto* E = CastChecked<ACandyFightEnemyCharacter>(GetPawn()); const auto& C = *E->EnemyConfig;
    if (Now-SearchStarted >= C.SearchSeconds) { BeginReturn(TEXT("搜索结束，未发现玩家")); return; }
    if (bHasCombatDestination && GetMoveStatus() == EPathFollowingStatus::Moving) return;
    if (SearchPointsUsed < C.SearchPointCount)
    {
        ++SearchPointsUsed;
        for (int32 I=0; I<8; ++I)
        {
            FVector Candidate = LastKnownLocation + FRotator(0,(GetUniqueID()%8+I+SearchPointsUsed*3)*45.,0).Vector()*C.SearchRadius;
            Candidate.Z = E->GetNavAgentLocation().Z;
            FVector Projected; FNavPathSharedPtr Path;
            if (CandyFightPositioning::FindPath(*E,Candidate,ReturnAnchor,C.LeashRadius,Projected,Path) &&
                FVector::Dist2D(Projected,LastKnownLocation)<=C.SearchRadius &&
                FVector::Dist2D(Projected,E->GetNavAgentLocation())>C.DestinationTolerance && RequestCombatMove(Projected,Path)) return;
        }
    }
    E->AddActorWorldRotation(FRotator(0,C.SearchTurnSpeed*DeltaSeconds,0));
}
void ACandyFightEnemyAIController::BeginReturn(const FString& Reason)
{
    if (!bEngaged || CombatState == ECandyFightEnemyState::Return) return;
    bTargetVisible = false; HaltCombatMove(); SetCombatState(ECandyFightEnemyState::Return,Reason);
    ReturnFailures=0; NextReturnRetry=GetWorld()->GetTimeSeconds();
    auto* E=CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    E->GetCharacterMovement()->MaxWalkSpeed=E->EnemyConfig->CombatWalkSpeed;
    LastCombatProgress=NextReturnRetry;
}
void ACandyFightEnemyAIController::FinishReturn()
{
    HaltCombatMove(); bEngaged=false; CombatTarget.Reset(); bTargetVisible=false;
    SetCombatState(ECandyFightEnemyState::Patrol,TEXT("归位完成，恢复原巡逻进度"));
    auto* E=CastChecked<ACandyFightEnemyCharacter>(GetPawn());
    E->GetCharacterMovement()->MaxWalkSpeed=E->GetPatrolConfig().WalkSpeed;
    ResumePatrolAfterWeaponTest();
    NextSenseTime=GetWorld()->GetTimeSeconds()+E->EnemyConfig->PerceptionInterval;
}
void ACandyFightEnemyAIController::UpdateReturn(double Now)
{
    auto* E=CastChecked<ACandyFightEnemyCharacter>(GetPawn()); const auto& C=*E->EnemyConfig;
    if (FVector::Dist2D(E->GetNavAgentLocation(),ReturnAnchor)<=C.DestinationTolerance ||
        (bHasCombatDestination && FVector::Dist2D(E->GetNavAgentLocation(),CombatDestination)<=C.DestinationTolerance)) { FinishReturn(); return; }
    if (bHasCombatDestination && GetMoveStatus()==EPathFollowingStatus::Moving && Now-LastCombatProgress<C.CombatStuckTimeout) return;
    if (Now<NextReturnRetry) return;
    if (bHasCombatDestination || bLastMoveFailed) ++ReturnFailures;
    bLastMoveFailed = false;
    HaltCombatMove(); NextReturnRetry=Now+C.ReturnRetrySeconds;
    FVector Destination; FNavPathSharedPtr Path;
    if (ReturnFailures<C.ReturnFailuresBeforeFallback && CandyFightPositioning::FindPath(*E,ReturnAnchor,ReturnAnchor,C.LeashRadius,Destination,Path) && RequestCombatMove(Destination,Path))
        { LastCombatProgress=Now; return; }
    ++ReturnFailures;
    if (ReturnFailures>=C.ReturnFailuresBeforeFallback)
    {
        // 优先选择距离原归位点最近的完整可达备选点。
        for (float Fraction : {.25f,.5f,.75f,1.f}) for (int32 I=0; I<8; ++I)
        {
            const FVector Candidate=ReturnAnchor+FRotator(0,I*45.,0).Vector()*C.ReturnFallbackRadius*Fraction;
            if (CandyFightPositioning::FindPath(*E,Candidate,ReturnAnchor,C.LeashRadius,Destination,Path) &&
                FVector::Dist2D(Destination,ReturnAnchor)<=C.ReturnFallbackRadius && RequestCombatMove(Destination,Path))
                { LastCombatProgress=Now; return; }
        }
    }
    StopReason=TEXT("归位路径不可达，原地等待重试");
}
bool ACandyFightEnemyAIController::BeginManualTest()
{
    bManualFromEngagement=bEngaged;
    if (!SuspendPatrolForWeaponTest()) return false;
    if (bEngaged) HaltCombatMove();
    SetCombatState(ECandyFightEnemyState::ManualTest,TEXT("控制台测试接管"));
    return true;
}
void ACandyFightEnemyAIController::EndManualTest()
{
    if (bManualFromEngagement) BeginReturn(TEXT("测试结束，先归位"));
    else { SetCombatState(ECandyFightEnemyState::Patrol,TEXT("测试结束")); ResumePatrolAfterWeaponTest(); }
    bManualFromEngagement=false;
}
void ACandyFightEnemyAIController::UpdateAutonomy(float DeltaSeconds)
{
    if (LastAutonomyFrame==GFrameCounter || UGameplayStatics::IsGamePaused(this)) return;
    LastAutonomyFrame=GFrameCounter;
    auto* E=Cast<ACandyFightEnemyCharacter>(GetPawn());
    if (!E || CombatState==ECandyFightEnemyState::ManualTest) return;
    const double Now=GetWorld()->GetTimeSeconds();
    const bool bAutonomyEnabled = AutonomyEnabled();
    if (!bAutonomyEnabled) { if (bEngaged) BeginReturn(TEXT("自主战斗已关闭或参数非法")); else return; }
    const auto& C = E->EnemyConfig && E->EnemyConfig->IsValidAutonomy() ? *E->EnemyConfig : *GetDefault<UCandyFightEnemyConfig>();
    if (bEngaged && FVector::DistSquared2D(E->GetActorLocation(),CombatProgressLocation)>=400.)
        { CombatProgressLocation=E->GetActorLocation(); LastCombatProgress=Now; }
    if (CombatPath.IsValid() && !CombatPath->IsValid()) { bLastMoveFailed=true; HaltCombatMove(); }
    if (bAutonomyEnabled && Now>=NextSenseTime)
    {
        NextSenseTime=Now+C.PerceptionInterval;
        SensePlayer(Now);
    }
    if (bEngaged && CombatState!=ECandyFightEnemyState::Return)
    {
        if (!CombatTarget.IsValid()) BeginReturn(TEXT("玩家失效"));
        else if (Now-LastClueTime>=C.ClueTimeout) BeginReturn(TEXT("连续无新线索超时"));
        else if (!CandyFightPositioning::InsideLeash(E->GetNavAgentLocation(),ReturnAnchor,C.LeashRadius)) BeginReturn(TEXT("到达追踪范围边界"));
        else if (CombatState!=ECandyFightEnemyState::Fire && CombatState!=ECandyFightEnemyState::Search && Now-LastCombatProgress>=C.CombatStuckTimeout)
            BeginReturn(TEXT("移动或寻位持续无进展"));
    }
    switch (CombatState)
    {
        case ECandyFightEnemyState::Position: case ECandyFightEnemyState::Fire: UpdatePosition(Now); break;
        case ECandyFightEnemyState::Investigate: UpdateInvestigation(Now); break;
        case ECandyFightEnemyState::Search: UpdateSearch(Now,DeltaSeconds); break;
        case ECandyFightEnemyState::Return: UpdateReturn(Now); break;
        default: break;
    }
    SyncCombatBlackboard();
#if !UE_BUILD_SHIPPING
    if (CVarCandyFightAIDebug.GetValueOnGameThread())
    {
        const FVector P=E->GetActorLocation();
        DrawDebugCircle(GetWorld(),P,C.HearingDistance,48,FColor::Cyan,false,0,0,1,FVector::ForwardVector,FVector::RightVector,false);
        for (float Sign : {-1.f,1.f}) DrawDebugLine(GetWorld(),P,P+E->GetActorForwardVector().RotateAngleAxis(Sign*C.SightAngleDegrees*.5,FVector::UpVector)*C.SightDistance,FColor::Green);
        if (bEngaged)
        {
            DrawDebugCircle(GetWorld(),ReturnAnchor+FVector(0,0,10),C.LeashRadius,64,FColor::Red,false,0,0,2,FVector::ForwardVector,FVector::RightVector,false);
            DrawDebugSphere(GetWorld(),LastKnownLocation,25,8,FColor::Yellow);
            if (bHasCombatDestination) DrawDebugLine(GetWorld(),P,CombatDestination,FColor::Blue);
        }
        FString DebugText=StaticEnum<ECandyFightEnemyState>()->GetNameStringByValue((int64)CombatState)+TEXT(" ")+StopReason;
        if (bHasCombatDestination)
            DebugText+=FString::Printf(TEXT("\n距站位 %.0f cm / 速度 %.0f / 移动状态 %d"),
                FVector::Dist2D(E->GetNavAgentLocation(),CombatDestination),E->GetVelocity().Size2D(),int32(GetMoveStatus()));
        DrawDebugString(GetWorld(),P+FVector(0,0,130),DebugText,nullptr,FColor::White,0);
    }
#endif
}
