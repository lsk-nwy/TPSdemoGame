#include "CandyFightAIEditorLibrary.h"
#if WITH_EDITOR
#include "CandyFightCombatPositioning.h"
#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyAIController.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFightEnemyPerceptionComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/CandyFightCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#endif

bool UCandyFightAIEditorLibrary::VerifyCombatNavigation(UObject* WorldContextObject)
{
#if WITH_EDITOR
    UWorld* World=WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || World->IsGameWorld() || World->GetOutermost()->GetName()!=TEXT("/Game/CandyFight/Maps/L_CandyFight_AI_Test")) return false;
    FActorSpawnParameters Spawn;
    Spawn.ObjectFlags=RF_Transient; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=World->SpawnActor<ACandyFightEnemyCharacter>(FVector(-1500,0,90),FRotator::ZeroRotator,Spawn);
    auto* Player=World->SpawnActor<ACandyFightCharacter>(FVector(-700,0,90),FRotator::ZeroRotator,Spawn);
    auto* Gun=World->SpawnActor<ACandyFightWeapon>(Spawn);
    if (!Enemy || !Player || !Gun)
    {
        if (Enemy) Enemy->Destroy(); if (Player) Player->Destroy(); if (Gun) Gun->Destroy(); return false;
    }
    Enemy->EnemyConfig=NewObject<UCandyFightEnemyConfig>(Enemy);
    auto* W=NewObject<UCandyFightWeaponConfig>(Enemy);
    Enemy->Combat->WeaponConfigs={W}; Enemy->Combat->EquippedIndex=0; Enemy->Combat->EquippedWeapon=Gun;
    Gun->AttachToComponent(Enemy->WeaponHoldPoint,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    World->UpdateWorldComponents(true,false);
    bool bPassed=true;
    auto Check=[&](const TCHAR* Name,bool bValue)
    {
        UE_LOG(LogTemp,Display,TEXT("CandyFight autonomy nav: %s = %s"),Name,bValue?TEXT("PASS"):TEXT("FAIL"));
        bPassed &= bValue;
    };
    FVector Point; FNavPathSharedPtr Path;
    const FVector Anchor=Enemy->GetNavAgentLocation();
    FVector ClueGround;
    FNavPathSharedPtr CluePath;
    Check(TEXT("clue ground reference reachable"), CandyFightPositioning::FindPath(*Enemy,FVector(-700,0,2),Anchor,2500,ClueGround,CluePath));
    FVector AirborneClue=ClueGround+FVector(0,0,Enemy->GetPatrolConfig().ProjectionDistance+2.f);
    const FVector SavedClue=AirborneClue;
    FString FailureReason;
    Check(TEXT("regression: strict ground query rejects muzzle-height point"),
        !CandyFightPositioning::FindPath(*Enemy,AirborneClue,Anchor,2500,Point,Path));
    for (float Height : {Enemy->GetPatrolConfig().ProjectionDistance+2.f,150.f,200.f})
    {
        const FVector Clue=ClueGround+FVector(0,0,Height);
        Check(TEXT("airborne clue resolves to complete ground path"),
            CandyFightPositioning::FindCluePath(*Enemy,Clue,Anchor,2500,Point,Path,&FailureReason) &&
            FVector::Dist(Point,ClueGround)<1.f && FailureReason.IsEmpty());
    }
    Check(TEXT("raw heard position remains unchanged"),AirborneClue==SavedClue);
    Check(TEXT("clue projection does not bypass leash"),
        !CandyFightPositioning::FindCluePath(*Enemy,AirborneClue,Anchor,200,Point,Path,&FailureReason) && !Path.IsValid() && !FailureReason.IsEmpty());
    Check(TEXT("clue projection does not snap arbitrary height to floor"),
        !CandyFightPositioning::FindCluePath(*Enemy,ClueGround+FVector(0,0,10000),Anchor,2500,Point,Path));
    Check(TEXT("native position selection on saved navmesh"),CandyFightPositioning::FindPosition(*Enemy,*Player,Anchor,600,1000,nullptr,Point,Path));
    if (Path.IsValid())
    {
        bool bInside=true;
        for(const auto& P:Path->GetPathPoints()) bInside &= CandyFightPositioning::InsideLeash(P.Location,Anchor,2500);
        Check(TEXT("entire selected path stays within leash"),bInside);
        FVector Muzzle,Center;
        Check(TEXT("candidate mount and muzzle visibility"),CandyFightPositioning::CandidateMuzzle(*Enemy,Point,Player->GetActorLocation(),Muzzle,Center) &&
            CandyFightPositioning::HasFiringLine(*Enemy,*Player,Muzzle,Center,W->Range));
        const FVector Saved=Enemy->GetActorLocation(); Enemy->SetActorLocation(Point+FVector(0,0,88));
        FVector Back; FNavPathSharedPtr BackPath;
        Check(TEXT("selected position has complete return path"),CandyFightPositioning::FindPath(*Enemy,Anchor,Anchor,2500,Back,BackPath));
        Enemy->SetActorLocation(Saved);
    }
    Path.Reset();
    Check(TEXT("destination inside leash cannot use outside starting path"),
        !CandyFightPositioning::FindPath(*Enemy,FVector(-700,0,2),FVector(-700,0,2),200,Point,Path));
    // L 墙两侧的直线被挡，但现有导航允许绕墙；不能把直线可达误当完整路径。
    Path.Reset();
    Check(TEXT("L-wall complete detour"),CandyFightPositioning::FindPath(*Enemy,FVector(700,0,2),Anchor,2500,Point,Path) &&
        Path->GetLength()>FVector::Dist2D(Anchor,Point)+100.f);
    // 同伴不遮挡视觉，却要阻挡枪口射线。
    auto* Ally=World->SpawnActor<ACandyFightEnemyCharacter>(FVector(-1100,0,90),FRotator::ZeroRotator,Spawn);
    Gun->AimAt(Player->GetActorLocation(),Enemy->GetActorForwardVector());
    Check(TEXT("ally does not block perception"),Enemy->Perception->CanSee(Player));
    Check(TEXT("ally blocks horizontal firing line"),!CandyFightPositioning::HasFiringLine(*Enemy,*Player,Gun->Muzzle->GetComponentLocation(),Enemy->GetActorLocation(),W->Range));
    if (Ally) Ally->Destroy();
    // 使用实际控制器发起请求但不推进世界、不启动 Play，覆盖枪声 → 导航目标的接线。
    auto* Controller=World->SpawnActor<ACandyFightEnemyAIController>(Spawn);
    if (Controller)
    {
        Controller->Possess(Enemy);
        Controller->ReturnAnchor=Anchor;
        Controller->LastKnownLocation=AirborneClue;
        Controller->CombatState=ECandyFightEnemyState::Investigate;
        Controller->StopReason=TEXT("调查点暂不可达");
        Controller->UpdateInvestigation(World->GetTimeSeconds());
        Check(TEXT("investigation accepts actual move request from airborne noise"),
            Controller->bHasCombatDestination && Controller->GetMoveStatus()==EPathFollowingStatus::Moving &&
            FVector::Dist(Controller->CombatDestination,ClueGround)<1.f);
        Check(TEXT("successful investigation clears stale failure text"),Controller->StopReason==TEXT("前往最后线索位置"));
        Check(TEXT("controller retains original sound position"),Controller->LastKnownLocation==AirborneClue);
        Controller->StopMovement();
        // 不启动 Play：使用保存的 NavMesh、真实 PathFollowing 到达判断及控制器状态转换，
        // 重现“导航已到达但距离还不能开火”，并验证步枪/霰弹接近与后退两个方向。
        for (bool bShotgun : {false,true}) for (bool bRetreat : {false,true})
        {
            W->FireMode=bShotgun ? ECandyFightFireMode::Shotgun : ECandyFightFireMode::SemiAutomatic;
            const float Min=bShotgun ? 300.f : 600.f, Max=bShotgun ? 600.f : 1000.f;
            const float Distance=bRetreat ? Min-50.f : Max+50.f;
            Controller->HaltCombatMove();
            Enemy->SetActorLocation(Player->GetActorLocation()-FVector(Distance,0,0));
            Enemy->SetActorRotation(FRotator::ZeroRotator);
            const FVector FeetToCenter=Enemy->GetActorLocation()-Enemy->GetNavAgentLocation();
            FVector OldGoal=Player->GetActorLocation()-FVector(bRetreat ? Min : Max,0,0);
            OldGoal.Z=Enemy->GetNavAgentLocation().Z;
            FVector OldProjected; FNavPathSharedPtr OldPath;
            Check(TEXT("old boundary destination has valid nav path"),
                CandyFightPositioning::FindPath(*Enemy,OldGoal,Anchor,2500,OldProjected,OldPath));
            FAIMoveRequest OldRequest(OldProjected);
            OldRequest.SetAcceptanceRadius(75.f);
            OldRequest.SetReachTestIncludesAgentRadius(false); OldRequest.SetReachTestIncludesGoalRadius(false);
            Check(TEXT("regression: old navigation accepts stop outside firing distances"),
                Controller->GetPathFollowingComponent()->HasReached(OldRequest) && (Distance<Min || Distance>Max));
            Controller->CombatTarget=Player; Controller->bTargetVisible=true;
            Controller->CombatState=ECandyFightEnemyState::Position; Controller->NextPositionTime=0.;
            Controller->UpdatePosition(World->GetTimeSeconds());
            const float Acceptance=CandyFightPositioning::PositionAcceptanceRadius(*Enemy,Min,Max);
            Check(TEXT("position request uses precise arrival and still moves"),
                Controller->bHasCombatDestination && Controller->GetMoveStatus()==EPathFollowingStatus::Moving &&
                FMath::IsNearlyEqual(Controller->GetPathFollowingComponent()->GetAcceptanceRadius(),Acceptance));
            const FVector Goal=Controller->CombatDestination;
            const float GoalDistance=FVector::Dist2D(Goal,Player->GetActorLocation());
            Check(TEXT("selected goal leaves arrival tolerance inside firing band"),
                GoalDistance-Acceptance>=Min && GoalDistance+Acceptance<=Max);
            const FVector Radial=(Goal-Player->GetActorLocation()).GetSafeNormal2D();
            for (float Sign : {-1.f,1.f})
            {
                Enemy->SetActorLocation(Goal+Radial*(Sign*Acceptance*.95f)+FeetToCenter);
                FAIMoveRequest NewRequest(Goal); NewRequest.SetAcceptanceRadius(Acceptance);
                NewRequest.SetReachTestIncludesAgentRadius(false); NewRequest.SetReachTestIncludesGoalRadius(false);
                Check(TEXT("new arrival boundary accepted by real PathFollowing"),Controller->GetPathFollowingComponent()->HasReached(NewRequest));
                Controller->CombatState=ECandyFightEnemyState::Position;
                Controller->UpdatePosition(World->GetTimeSeconds());
                Check(TEXT("accepted arrival transitions to Fire with valid muzzle line"),
                    Controller->CombatState==ECandyFightEnemyState::Fire);
            }
        }
        Controller->HaltCombatMove();
        Enemy->Combat->EquippedWeapon=nullptr;
        Controller->UnPossess(); Controller->Destroy();
    }
    else Check(TEXT("investigation controller fixture"),false);
    Enemy->Combat->EquippedWeapon=nullptr;
    Gun->Destroy(); Player->Destroy(); Enemy->Destroy();
    return bPassed;
#else
    return false;
#endif
}
