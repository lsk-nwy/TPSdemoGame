#if WITH_DEV_AUTOMATION_TESTS
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/AI/CandyFightEnemyAIController.h"
#include "CandyFight/AI/CandyFightEnemyConfig.h"
#include "CandyFight/AI/CandyFightEnemyWeaponComponent.h"
#include "CandyFight/AI/CandyFightPatrolBrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/CandyFightCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDeviceNull.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEnemyBurstTest, "CandyFight.AI.Weapons.BurstRules", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightEnemyBurstTest::RunTest(const FString& Parameters)
{
    auto* Config = NewObject<UCandyFightEnemyConfig>();
    TestTrue(TEXT("Defaults valid"), Config->IsValidWeaponDriver());
    Config->BurstShotCount=0; TestFalse(TEXT("Zero group rejected"),Config->IsValidWeaponDriver());
    Config->BurstShotCount=5; Config->BurstPauseSeconds=0; TestTrue(TEXT("Zero pause valid"),Config->IsValidWeaponDriver());
    Config->BurstPauseSeconds=std::numeric_limits<float>::infinity(); TestFalse(TEXT("Infinite pause rejected"),Config->IsValidWeaponDriver());
    FCandyFightBurstState Burst;
    for(int32 I=0; I<5; ++I) Burst.CommitShot(5,1.,false,I*.1);
    TestEqual(TEXT("Five shots close one group"),Burst.CompletedGroups,1);
    TestTrue(TEXT("Pause measured from last shot"),FMath::IsNearlyEqual(Burst.NextGroupTime,1.4));
    Burst.Stop(); TestTrue(TEXT("Stop preserves cooldown"),FMath::IsNearlyEqual(Burst.NextGroupTime,1.4));
    Burst.CommitShot(5,1.,true,2.);
    TestEqual(TEXT("Empty magazine closes partial group"),Burst.CompletedGroups,2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightEnemyWeaponIntegrationTest,"CandyFight.AI.Weapons.DriverAndLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightEnemyWeaponIntegrationTest::RunTest(const FString& Parameters)
{
    TGuardValue<uint64> RestoreFrame(GFrameCounter,GFrameCounter);
    for (auto Mode : {ECandyFightFireMode::SemiAutomatic,ECandyFightFireMode::Automatic,ECandyFightFireMode::Shotgun})
    for (int32 FPS : {30,60,120})
    {
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
        World->CreateAISystem();
        auto* Enemy=World->SpawnActor<ACandyFightEnemyCharacter>();
        auto* AI=World->SpawnActor<ACandyFightEnemyAIController>();
        Enemy->EnemyConfig=NewObject<UCandyFightEnemyConfig>(Enemy);
        auto* Config=NewObject<UCandyFightWeaponConfig>(Enemy);
        Config->WeaponClass=ACandyFightWeapon::StaticClass();
        Config->FireMode=Mode; Config->MagazineCapacity=7; Config->ShotsPerSecond=10.f;
        Config->ReloadSeconds=.3f; Config->ShellReloadStartDelay=.4f;
        Config->PelletCount=Mode==ECandyFightFireMode::Shotgun?12:1;
        Config->Damage=Mode==ECandyFightFireMode::Shotgun?8.f:20.f;
        Config->BaseSpread=Config->MaxSpread=0.f;
        Enemy->WeaponOverride=Config;
        AI->Possess(Enemy);
        auto* Combat=Enemy->Combat.Get(); auto* Driver=Enemy->WeaponDriver.Get();
        Combat->BeginPlay(); Driver->BeginPlay();
        auto* Target=World->SpawnActor<ACandyFightTarget>(FVector(2000,0,110),FRotator::ZeroRotator);
        FString Error;
        TestFalse(TEXT("No target rejected"),Driver->StartTest(Error));
        TestFalse(TEXT("Invalid start leaves patrol running"),AI->IsPatrolSuspended());
        Enemy->TestTarget=Target;
        TestTrue(TEXT("Valid start"),Driver->StartTest(Error));
        TestTrue(TEXT("Patrol suspended"),AI->IsPatrolSuspended());
        TestNull(TEXT("AI creates no HUD"),Combat->WeaponHUD.Get());
        if(Mode==ECandyFightFireMode::Automatic && FPS==60)
        {
            Driver->StopTest();
            Enemy->TestId=1;
            auto* Duplicate=World->SpawnActor<ACandyFightEnemyCharacter>(); Duplicate->TestId=1;
            FOutputDeviceNull Output;
            IConsoleManager::Get().ProcessUserConsoleInput(TEXT("EnemyWeaponTestStart 1"),Output,World);
            TestFalse(TEXT("Duplicate ID cannot start"),Driver->IsTesting());
            Duplicate->Destroy();
            IConsoleManager::Get().ProcessUserConsoleInput(TEXT("EnemyWeaponTestStart -1"),Output,World);
            TestFalse(TEXT("Invalid ID cannot start"),Driver->IsTesting());
            IConsoleManager::Get().ProcessUserConsoleInput(TEXT("EnemyWeaponTestStart 1"),Output,World);
            TestTrue(TEXT("Console starts matching enemy"),Driver->IsTesting());
        }
        int32 PreviousShots=0; int32 GroupShots=0; double LastShot=-100.,NextGroup=0.;
        for(int32 Frame=0;Frame<FPS*7;++Frame)
        {
            ++GFrameCounter; World->TimeSeconds=double(Frame)/FPS;
            const double Now=World->TimeSeconds;
            Driver->UpdateDriver(Now);
            Combat->TickComponent(1.f/FPS,LEVELTICK_All,nullptr);
            const int32 Shots=Combat->TotalShotsFired;
            TestTrue(TEXT("At most one shot per frame"),Shots-PreviousShots<=1);
            if(Shots!=PreviousShots)
            {
                TestTrue(TEXT("Group pause observed"),Now+1.e-5>=NextGroup);
                TestTrue(TEXT("Weapon rate respected within one frame"),Now-LastShot+1./FPS+1.e-4>=.1);
                ++GroupShots; LastShot=Now;
                if(GroupShots==5 || Combat->GetAmmo()==0) { NextGroup=Now+1.; GroupShots=0; }
                TestEqual(TEXT("One gunshot request per shot"),Combat->FireSoundRequests,Shots);
                TestEqual(TEXT("Pellet count"),Combat->LastShotEnds.Num(),Config->PelletCount);
                for(const FVector& End:Combat->LastShotEnds) TestTrue(TEXT("Horizontal flight"),FMath::IsNearlyEqual(End.Z,Combat->LastShotStart.Z));
                if(Combat->GetAmmo()==0) TestTrue(TEXT("Empty starts reload"),Combat->IsReloading());
            }
            TestFalse(TEXT("AI never dry fires"),Combat->IsDryFiring());
            if(Frame==FPS/2) TestTrue(TEXT("Repeated start is harmless"),Driver->StartTest(Error));
            PreviousShots=Shots;
        }
        TestTrue(TEXT("Multiple groups fire"),Combat->TotalShotsFired>=10);
        TestEqual(TEXT("No dry sound requests"),Combat->DrySoundRequests,0);
        TestEqual(TEXT("Pause is not a patrol failure"),AI->FailedAttempts,0);
        AI->Tick(30.f); TestEqual(TEXT("Paused watchdog cannot abort"),AI->WatchdogAborts,0);
        if(!Combat->ActiveShots.IsEmpty() && Combat->ActiveShots.Last().IsValid())
        {
            auto* FlyingShot=Combat->ActiveShots.Last().Get();
            Target->Health=1000.f;
            const float ExpectedDamage=Config->Damage*Config->PelletCount;
            const float SavedDamage=Config->Damage;
            Config->Damage=999.f; // 已发射子弹必须保持原参数快照。
            FlyingShot->AdvanceTo(World->TimeSeconds+1.);
            TestEqual(TEXT("Enemy shot uses damage snapshot and merges same-frame pellets"),Target->Health,1000.f-ExpectedDamage);
            TestEqual(TEXT("One merged damage event per enemy shot arrival"),Target->HitCount,1);
            Config->Damage=SavedDamage;
        }
        const int32 BeforeStop=Combat->TotalShotsFired;
        Driver->UpdateDriver(World->TimeSeconds); // A request may be pending before Stop.
        Driver->StopTest();
        ++GFrameCounter; Combat->TickComponent(.01f,LEVELTICK_All,nullptr);
        TestEqual(TEXT("Stop cancels pending press"),Combat->TotalShotsFired,BeforeStop);
        TestFalse(TEXT("Patrol resumes"),AI->IsPatrolSuspended());
        TestTrue(TEXT("Movement facing restored"),Enemy->GetCharacterMovement()->bOrientRotationToMovement);
        const int32 SavedAmmo=Combat->GetAmmo();
        TestTrue(TEXT("Restart"),Driver->StartTest(Error));
        TestEqual(TEXT("Restart preserves ammo"),Combat->GetAmmo(),SavedAmmo);
        World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
        Driver->UpdateDriver(World->TimeSeconds);
        TestEqual(TEXT("Paused driver adds no shots"),Combat->TotalShotsFired,BeforeStop);
        World->GetWorldSettings()->SetPauserPlayerState(nullptr);
        Target->Destroy(); Driver->UpdateDriver(World->TimeSeconds);
        TestFalse(TEXT("Destroyed target stops test"),Driver->IsTesting());
        TestFalse(TEXT("Destroyed target resumes patrol"),AI->IsPatrolSuspended());
        AI->UnPossess();
        TestNull(TEXT("Unpossess destroys gun"),Combat->EquippedWeapon.Get());
        TestTrue(TEXT("Unpossess clears shots"),Combat->ActiveShots.IsEmpty());
        Driver->EndPlay(EEndPlayReason::Destroyed);
        Combat->EndPlay(EEndPlayReason::Destroyed);
        World->DestroyWorld(false);
    }
    // 使用真实 BT 的 RetryDelay 等待任务，验证暂停期间不消耗剩余等待。
    {
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
        World->CreateAISystem();
        auto* Enemy=World->SpawnActor<ACandyFightEnemyCharacter>();
        Enemy->EnemyConfig=NewObject<UCandyFightEnemyConfig>(Enemy);
        Enemy->PatrolBehaviorTree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightEnemy.BT_CandyFightEnemy"));
        auto* AI=World->SpawnActor<ACandyFightEnemyAIController>();
        AI->Possess(Enemy);
        auto* Brain=Cast<UCandyFightPatrolBrainComponent>(AI->GetBrainComponent());
        TestNotNull(TEXT("Custom patrol brain"),Brain);
        for(int32 I=0;I<5;++I) { ++GFrameCounter; World->TimeSeconds=.1*I; Brain->TickComponent(.1f,LEVELTICK_All,nullptr); }
        AddInfo(TEXT("Before pause: ")+Brain->GetDebugInfoString());
        const int32 Failures=AI->FailedAttempts;
        TestTrue(TEXT("Real tree entered retry after unavailable nav"),Failures>0);
        AI->SuspendPatrolForWeaponTest();
        ++GFrameCounter; World->TimeSeconds+=10.;
        Brain->TickComponent(10.f,LEVELTICK_All,nullptr);
        TestEqual(TEXT("Frozen tree does not run selection"),AI->FailedAttempts,Failures);
        AI->ResumePatrolAfterWeaponTest();
        ++GFrameCounter; World->TimeSeconds+=.1;
        Brain->TickComponent(10.1f,LEVELTICK_All,nullptr);
        TestEqual(TEXT("Resume excludes paused delta from Wait"),AI->FailedAttempts,Failures);
        for(int32 I=0;I<65;++I) { ++GFrameCounter; World->TimeSeconds+=.1; Brain->TickComponent(.1f,LEVELTICK_All,nullptr); }
        AddInfo(TEXT("After resume: ")+Brain->GetDebugInfoString());
        TestTrue(TEXT("Remaining wait eventually expires"),AI->FailedAttempts>Failures);
        AI->GetBrainComponent()->StopLogic(TEXT("Move-only test"));
        auto Path=MakeShared<FNavigationPath,ESPMode::ThreadSafe>();
        Path->GetPathPoints().Add(FNavPathPoint(Enemy->GetActorLocation()));
        Path->GetPathPoints().Add(FNavPathPoint(FVector(1000,0,0)));
        Path->MarkReady(); Path->DoneUpdating(ENavPathUpdateType::GoalMoved);
        FAIMoveRequest Request(FVector(1000,0,0)); Request.SetUsePathfinding(false);
        const FAIRequestID MoveId=AI->RequestMove(Request,Path);
        TestTrue(TEXT("Synthetic route movement starts"),MoveId.IsValid());
        const int32 BeforeFailure=AI->FailedAttempts;
        AI->SuspendPatrolForWeaponTest();
        TestEqual(TEXT("Movement request paused"),AI->GetMoveStatus(),EPathFollowingStatus::Paused);
        World->TimeSeconds+=3.; AI->Tick(3.f);
        AI->ResumePatrolAfterWeaponTest();
        TestEqual(TEXT("Movement request resumes"),AI->GetMoveStatus(),EPathFollowingStatus::Moving);
        TestEqual(TEXT("Pause/resume adds no failure"),AI->FailedAttempts,BeforeFailure);
        AI->bHasTarget=true;
        AI->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolLocation"),FVector(1000,0,0));
        AI->PatrolIndex=2; AI->PatrolDirection=-1;
        AI->SuspendPatrolForWeaponTest();
        Path->Invalidate();
        AI->ResumePatrolAfterWeaponTest();
        TestEqual(TEXT("Lost path during test is not immediately a failed attempt"),AI->FailedAttempts,BeforeFailure);
        TestEqual(TEXT("Route index preserved after lost path"),AI->PatrolIndex,2);
        TestEqual(TEXT("Ping-pong direction preserved"),AI->PatrolDirection,-1);
        TestFalse(TEXT("Retry checks actual reachability"),AI->SelectPatrolLocation());
        TestEqual(TEXT("Only real unavailable navigation increments failure"),AI->FailedAttempts,BeforeFailure+1);
        AI->UnPossess(); World->DestroyWorld(false);
    }
    return true;
}
#endif
