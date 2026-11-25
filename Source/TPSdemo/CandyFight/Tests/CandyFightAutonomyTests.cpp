#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CandyFight/AI/CandyFightEnemyAIController.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/AI/CandyFightEnemyConfig.h"
#include "CandyFight/AI/CandyFightEnemyPerceptionComponent.h"
#include "CandyFight/AI/CandyFightEnemyWeaponComponent.h"
#include "CandyFight/AI/CandyFightCombatPositioning.h"
#include "CandyFight/AI/CandyFightPatrolBrainComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/CandyFightCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPerceptionMathTest,"CandyFight.AI.Autonomy.ConfigAndGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightPerceptionMathTest::RunTest(const FString& Parameters)
{
    auto* C=NewObject<UCandyFightEnemyConfig>();
    TestTrue(TEXT("Default autonomy parameters valid"),C->IsValidAutonomy());
    C->SightAngleDegrees=361; TestFalse(TEXT("Invalid cone rejected"),C->IsValidAutonomy()); C->SightAngleDegrees=120;
    C->SearchPointCount=-1; TestFalse(TEXT("Negative search count rejected"),C->IsValidAutonomy()); C->SearchPointCount=0;
    TestTrue(TEXT("Stationary search can be configured"),C->IsValidAutonomy());
    C->PositionInterval=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("NaN rejected"),C->IsValidAutonomy());
    using P=UCandyFightEnemyPerceptionComponent;
    TestTrue(TEXT("Cone forward"),P::InSightCone(FVector::ZeroVector,FVector::ForwardVector,FVector(1500,0,0),1500,120));
    TestFalse(TEXT("Cone rear"),P::InSightCone(FVector::ZeroVector,FVector::ForwardVector,FVector(-100,0,0),1500,120));
    TestTrue(TEXT("Cone boundary included"),P::InSightCone(FVector::ZeroVector,FVector::ForwardVector,FRotator(0,60,0).Vector()*1000,1500,120));
    TestFalse(TEXT("Outside cone"),P::InSightCone(FVector::ZeroVector,FVector::ForwardVector,FRotator(0,61,0).Vector()*1000,1500,120));
    TestTrue(TEXT("Sight angle is horizontal"),P::InSightCone(FVector::ZeroVector,FVector::ForwardVector,FVector(500,0,500),1500,120));
    TestFalse(TEXT("Beyond distance"),P::InSightCone(FVector::ZeroVector,FVector::ForwardVector,FVector(1501,0,0),1500,120));
    TestTrue(TEXT("Leash boundary"),CandyFightPositioning::InsideLeash(FVector(2500,0,200),FVector::ZeroVector,2500));
    TestFalse(TEXT("Leash beyond"),CandyFightPositioning::InsideLeash(FVector(2501,0,0),FVector::ZeroVector,2500));
    FCandyFightBurstState Burst; Burst.CommitShot(5,1,false,2); Burst.Interrupt(1);
    TestEqual(TEXT("Interrupted group cleared"),Burst.ShotsInGroup,0);
    TestEqual(TEXT("Interrupted pause from last real shot"),Burst.NextGroupTime,3.);
    Burst.Interrupt(1); TestEqual(TEXT("Repeated interruption does not extend pause"),Burst.NextGroupTime,3.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightAutonomyTest,"CandyFight.AI.Autonomy.SensingCombatReturnLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightAutonomyTest::RunTest(const FString& Parameters)
{
    TGuardValue<uint64> RestoreFrame(GFrameCounter,GFrameCounter);
    for (int32 FPS : {30,60,120})
    {
        auto* World=UWorld::CreateWorld(EWorldType::Game,false); World->CreateAISystem();
        auto* E=World->SpawnActor<ACandyFightEnemyCharacter>(FVector(0,0,88),FRotator::ZeroRotator);
        auto* C=NewObject<UCandyFightEnemyConfig>(E); E->EnemyConfig=C;
        C->AutonomyTree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightCombat"));
        E->PatrolBehaviorTree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightEnemy"));
        auto* W=NewObject<UCandyFightWeaponConfig>(E); W->WeaponClass=ACandyFightWeapon::StaticClass();
        W->FireMode=ECandyFightFireMode::Automatic; W->ShotsPerSecond=10; W->MagazineCapacity=30;
        W->BaseSpread=W->MaxSpread=0; E->WeaponOverride=W;
        auto* AI=World->SpawnActor<ACandyFightEnemyAIController>(); AI->Possess(E);
        E->Combat->BeginPlay(); E->WeaponDriver->BeginPlay();
        auto* Player=World->SpawnActor<ACandyFightCharacter>(FVector(800,0,88),FRotator::ZeroRotator);
        auto* PC=World->SpawnActor<APlayerController>(); PC->Possess(Player);
        PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
        TestTrue(TEXT("Fixture player lookup"),UGameplayStatics::GetPlayerPawn(E,0)==Player);
        TestTrue(TEXT("Fixture initial sight"),E->Perception->CanSee(Player));
        auto Step=[&](double Time)
        {
            World->TimeSeconds=Time; ++GFrameCounter; AI->UpdateAutonomy(1.f/FPS);
            E->WeaponDriver->UpdateDriver(Time); E->Combat->TickComponent(1.f/FPS,LEVELTICK_All,nullptr);
        };
        UBehaviorTree* SavedTree=C->AutonomyTree;
        C->AutonomyTree=nullptr;
        AI->ReceivePlayerShot(Player,FVector(800,0,108));
        TestFalse(TEXT("Cleared combat tree cannot suspend patrol on noise"),AI->IsPatrolSuspended());
        C->AutonomyTree=SavedTree;
        AI->NextSenseTime=0.;
        Step(0.);
        TestTrue(TEXT("Sees local player"),AI->bTargetVisible);
        TestEqual(TEXT("Good current location stops to fire"),AI->CombatState,ECandyFightEnemyState::Fire);
        TestTrue(TEXT("Final muzzle check allows unobstructed target"),AI->CanAutonomousFire());
        const FVector Anchor=AI->ReturnAnchor;
        for(int I=1; I<=FPS; ++I) Step(double(I)/FPS);
        TestEqual(TEXT("Only five successful shots before group pause"),E->Combat->TotalShotsFired,5);
        const int32 Before=E->Combat->TotalShotsFired;
        AI->UpdateAutonomy(5); E->Combat->TickComponent(1.f/FPS,LEVELTICK_All,nullptr);
        TestEqual(TEXT("Same frame cannot duplicate shot"),E->Combat->TotalShotsFired,Before);
        // 真碰撞墙：视觉和水平枪口路径都阻挡。
        auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
        Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(10,300,300));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent(); Wall->SetActorLocation(FVector(400,0,100));
        TestFalse(TEXT("Wall blocks sight"),E->Perception->CanSee(Player));
        TestFalse(TEXT("Final same-frame gate blocks gunfire"),AI->CanAutonomousFire());
        Step(1.2);
        TestEqual(TEXT("Sight loss investigates last known location"),AI->CombatState,ECandyFightEnemyState::Investigate);
        FVector Remembered=AI->LastKnownLocation;
        Player->SetActorLocation(FVector(1100,100,88)); Step(1.4);
        TestEqual(TEXT("Hidden movement does not update memory"),AI->LastKnownLocation,Remembered);
        const FVector Noise(900,30,108);
        UCandyFightEnemyPerceptionComponent::ReportPlayerShot(Player,Noise);
        TestEqual(TEXT("Hears successful player shot through wall"),AI->LastKnownLocation,Noise);
        TestFalse(TEXT("Hearing alone cannot fire"),AI->CanAutonomousFire());
        const double NoiseTime=AI->LastClueTime;
        UCandyFightEnemyPerceptionComponent::ReportPlayerShot(E,FVector(500,500,88));
        TestEqual(TEXT("Enemy shots produce no hearing clue"),AI->LastKnownLocation,Noise);
        AI->ReceivePlayerShot(Player,FVector(3000,0,88));
        TestEqual(TEXT("Out-of-range noise ignored"),AI->LastClueTime,NoiseTime);
        World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
        ++GFrameCounter; AI->UpdateAutonomy(20.f);
        TestEqual(TEXT("Pause freezes state"),AI->CombatState,ECandyFightEnemyState::Investigate);
        World->GetWorldSettings()->SetPauserPlayerState(nullptr);
        // 就算持续有新声音，走不动达到独立超时也必须归位。
        E->SetActorLocation(FVector(100,0,88)); AI->CombatProgressLocation=E->GetActorLocation();
        AI->LastCombatProgress=0.; AI->LastClueTime=5.; Step(5.1);
        TestEqual(TEXT("Fresh clues do not bypass no-progress timeout"),AI->CombatState,ECandyFightEnemyState::Return);
        World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
        AI->ReceivePlayerShot(Player,FVector(900,50,108));
        TestEqual(TEXT("Paused return ignores noise"),AI->CombatState,ECandyFightEnemyState::Return);
        World->GetWorldSettings()->SetPauserPlayerState(nullptr);
        C->bEnableAutonomousCombat=false;
        AI->ReceivePlayerShot(Player,FVector(900,50,108));
        TestEqual(TEXT("Disabled autonomy ignores return noise"),AI->CombatState,ECandyFightEnemyState::Return);
        C->bEnableAutonomousCombat=true;
        C->HearingDistance=4000;
        AI->ReceivePlayerShot(Player,FVector(2600,0,108));
        TestEqual(TEXT("Noise beyond original leash cannot interrupt return"),AI->CombatState,ECandyFightEnemyState::Return);
        C->HearingDistance=2000;
        const FVector ReturnNoise(900,50,108);
        AI->ReceivePlayerShot(Player,ReturnNoise);
        TestEqual(TEXT("Return hears noise through wall and investigates"),AI->CombatState,ECandyFightEnemyState::Investigate);
        TestEqual(TEXT("New gunshot updates clue position"),AI->LastKnownLocation,ReturnNoise);
        TestEqual(TEXT("Reacquisition resets no-progress clock"),AI->LastCombatProgress,World->GetTimeSeconds());
        TestFalse(TEXT("Hearing during return does not enable blind firing"),AI->CanAutonomousFire());
        TestTrue(TEXT("Reacquisition keeps original patrol paused"),AI->IsPatrolSuspended());
        TestEqual(TEXT("Anchor remains original"),AI->ReturnAnchor,Anchor);
        Step(5.15);
        TestEqual(TEXT("Old stuck timeout cannot immediately cancel investigation"),AI->CombatState,ECandyFightEnemyState::Investigate);
        AI->BeginReturn(TEXT("Test visual reacquisition"));
        Wall->Destroy(); E->SetActorRotation(FRotator(0,180,0)); AI->NextSenseTime=0.; Step(5.2);
        TestEqual(TEXT("Return still requires forward sight cone"),AI->CombatState,ECandyFightEnemyState::Return);
        E->SetActorRotation(FRotator::ZeroRotator); AI->NextSenseTime=0.; AI->LastCombatProgress=0.; Step(5.3);
        TestTrue(TEXT("Return reacquires visible player"),AI->bTargetVisible);
        TestTrue(TEXT("Sight resumes positioning or fire"),AI->CombatState==ECandyFightEnemyState::Position || AI->CombatState==ECandyFightEnemyState::Fire);
        TestEqual(TEXT("Visual reacquisition keeps original anchor"),AI->ReturnAnchor,Anchor);
        AI->BeginReturn(TEXT("Test disabled visual reacquisition"));
        C->bEnableAutonomousCombat=false; AI->NextSenseTime=0.; Step(5.4);
        TestEqual(TEXT("Disabled autonomy cannot visually reacquire during return"),AI->CombatState,ECandyFightEnemyState::Return);
        C->bEnableAutonomousCombat=true;
        Player->SetActorLocation(FVector(2600,0,88)); C->SightDistance=3000; AI->NextSenseTime=0.; Step(5.45);
        TestEqual(TEXT("Visible player beyond original leash cannot interrupt return"),AI->CombatState,ECandyFightEnemyState::Return);
        C->SightDistance=1500; Player->SetActorLocation(FVector(4000,0,88));
        E->SetActorLocation(FVector(Anchor.X,Anchor.Y,88)); Step(5.5);
        TestEqual(TEXT("Arrival restores patrol"),AI->CombatState,ECandyFightEnemyState::Patrol);
        TestFalse(TEXT("Original patrol pause released"),AI->IsPatrolSuspended());
        // 手动测试仍独占控制；结束后进入归位，之后可由新感知再次接敌。
        Player->SetActorLocation(FVector(800,0,88)); E->SetActorRotation(FRotator::ZeroRotator);
        Step(5.7); E->TestTarget=Player; FString Error;
        TestTrue(TEXT("Manual takeover starts"),E->WeaponDriver->StartTest(Error));
        TestEqual(TEXT("Manual tree branch"),AI->CombatState,ECandyFightEnemyState::ManualTest);
        AI->ReceivePlayerShot(Player,FVector(900,50,108));
        TestEqual(TEXT("Noise cannot interrupt manual test"),AI->CombatState,ECandyFightEnemyState::ManualTest);
        E->SetActorLocation(FVector(100,0,88)); E->WeaponDriver->StopTest();
        TestEqual(TEXT("Stopping autonomous takeover returns"),AI->CombatState,ECandyFightEnemyState::Return);
        // 3 秒搜索窗口及最多两个点，在没有导航时原地观察而不是乱走。
        AI->CombatState=ECandyFightEnemyState::Search; AI->SearchStarted=6.; AI->LastClueTime=6.; AI->NextSenseTime=100.;
        AI->bTargetVisible=false; AI->LastKnownLocation=FVector(100,0,88); AI->SearchPointsUsed=0;
        Step(6.); Step(6.5); Step(8.99);
        TestEqual(TEXT("Search limited to two attempts"),AI->SearchPointsUsed,2);
        TestEqual(TEXT("Search persists before deadline"),AI->CombatState,ECandyFightEnemyState::Search);
        Step(9.01); TestEqual(TEXT("Search expires at three seconds"),AI->CombatState,ECandyFightEnemyState::Return);
        AI->CombatState=ECandyFightEnemyState::Investigate; AI->LastClueTime=1.; AI->LastCombatProgress=9.;
        Step(9.1); TestEqual(TEXT("Eight-second clue budget expires during travel"),AI->CombatState,ECandyFightEnemyState::Return);
        AI->UnPossess();
        TestNull(TEXT("Lifecycle clears equipped gun"),E->Combat->EquippedWeapon.Get());
        TestFalse(TEXT("Lifecycle clears target"),AI->CombatTarget.IsValid());
        E->WeaponDriver->EndPlay(EEndPlayReason::Destroyed); E->Combat->EndPlay(EEndPlayReason::Destroyed);
        PC->UnPossess(); World->DestroyWorld(false);
    }
    // 两套实际树和黑板保持独立；暂停原 Wait，战斗树仍能运行。
    {
        auto* World=UWorld::CreateWorld(EWorldType::Game,false); World->CreateAISystem();
        auto* E=World->SpawnActor<ACandyFightEnemyCharacter>(); E->EnemyConfig=NewObject<UCandyFightEnemyConfig>(E);
        E->EnemyConfig->AutonomyTree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightCombat"));
        E->PatrolBehaviorTree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightEnemy"));
        auto* AI=World->SpawnActor<ACandyFightEnemyAIController>(); AI->Possess(E);
        TestNotNull(TEXT("Combat tree asset"),E->EnemyConfig->AutonomyTree.Get());
        TestNotNull(TEXT("Autonomy brain constructed"),AI->AutonomyBrain.Get());
        if (AI->AutonomyBrain)
        {
            TestTrue(TEXT("Patrol and autonomy blackboards distinct"),AI->GetBlackboardComponent()!=AI->AutonomyBlackboard.Get());
            TestTrue(TEXT("Patrol blackboard pairing"),AI->GetBrainComponent()->GetBlackboardComponent()==AI->GetBlackboardComponent());
            TestTrue(TEXT("Combat blackboard pairing"),AI->AutonomyBrain->GetBlackboardComponent()==AI->AutonomyBlackboard.Get());
            auto* Brain=CastChecked<UCandyFightPatrolBrainComponent>(AI->GetBrainComponent());
            for(int I=0;I<5;++I) { ++GFrameCounter; World->TimeSeconds=I*.1; Brain->TickComponent(.1f,LEVELTICK_All,nullptr); AI->AutonomyBrain->TickComponent(.1f,LEVELTICK_All,nullptr); }
            const int Failures=AI->FailedAttempts;
            AI->PatrolIndex=2; AI->PatrolDirection=-1;
            auto* Player=World->SpawnActor<APawn>();
            AI->EnterEngagement(Player,FVector(500,0,0),false);
            ++GFrameCounter; World->TimeSeconds+=10.; Brain->TickComponent(10,LEVELTICK_All,nullptr);
            TestEqual(TEXT("Combat pause preserves patrol wait"),AI->FailedAttempts,Failures);
            AI->FinishReturn();
            ++GFrameCounter; World->TimeSeconds+=.1; Brain->TickComponent(10.1f,LEVELTICK_All,nullptr);
            TestEqual(TEXT("Return excludes combat time from wait"),AI->FailedAttempts,Failures);
            TestEqual(TEXT("Patrol index preserved"),AI->PatrolIndex,2);
            TestEqual(TEXT("Patrol direction preserved"),AI->PatrolDirection,-1);
        }
        AI->UnPossess(); World->DestroyWorld(false);
    }
    return true;
}
#endif
