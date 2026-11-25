#if WITH_DEV_AUTOMATION_TESTS
#include "CandyFight/AI/CandyFightPatrolRoute.h"
#include "CandyFight/AI/CandyFightEnemyConfig.h"
#include "Misc/AutomationTest.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPatrolCursorTest, "CandyFight.AI.RouteBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightPatrolCursorTest::RunTest(const FString& Parameters)
{
	int32 Index = 0, Direction = 1;
	const int32 Expected[] = {1, 2, 1, 0, 1, 2, 1, 0};
	for (int32 Value : Expected)
	{
		ACandyFightPatrolRoute::AdvanceCursor(3, true, Index, Direction);
		TestEqual(TEXT("Bounce visits endpoints once"), Index, Value);
	}
	for (int32 I = 0; I < 12; ++I)
	{
		ACandyFightPatrolRoute::AdvanceCursor(3, false, Index, Direction);
		TestEqual(TEXT("Loop wraps"), Index, (I + 1) % 3);
	}
	for (int32 Count : {0, 1})
	{
		Index = 99; Direction = -1;
		ACandyFightPatrolRoute::AdvanceCursor(Count, true, Index, Direction);
		TestEqual(TEXT("Empty/single point index"), Index, 0);
		TestEqual(TEXT("Empty/single point direction"), Direction, 1);
	}
	int32 Failures = 0;
	TestFalse(TEXT("First failure retries"), ACandyFightPatrolRoute::RegisterFailure(3, Failures));
	TestFalse(TEXT("Second failure retries"), ACandyFightPatrolRoute::RegisterFailure(3, Failures));
	TestTrue(TEXT("Third failure skips"), ACandyFightPatrolRoute::RegisterFailure(3, Failures));
	TestEqual(TEXT("Next point starts fresh"), Failures, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPatrolConfigTest, "CandyFight.AI.Configuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightPatrolConfigTest::RunTest(const FString& Parameters)
{
	UCandyFightEnemyConfig* Config = NewObject<UCandyFightEnemyConfig>();
	TestTrue(TEXT("Defaults valid"), Config->IsValidConfiguration());
	Config->WaitMax = Config->WaitMin - 0.1f;
	TestFalse(TEXT("Reversed wait bounds rejected"), Config->IsValidConfiguration());
	Config->WaitMax = 3;
	Config->RetryDelay = 0;
	TestFalse(TEXT("Zero-delay busy loop rejected"), Config->IsValidConfiguration());
	Config->RetryDelay = 1;
	Config->MaxPointFailures = 0;
	TestFalse(TEXT("Unbounded retry configuration rejected"), Config->IsValidConfiguration());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightPatrolWeaponCompatibilityTest, "CandyFight.AI.WeaponCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCandyFightPatrolWeaponCompatibilityTest::RunTest(const FString& Parameters)
{
	UClass* EnemyClass = LoadClass<ACandyFightEnemyCharacter>(nullptr,
		TEXT("/Game/CandyFight/Blueprints/AI/BP_CandyFightEnemy.BP_CandyFightEnemy_C"));
	if (!TestNotNull(TEXT("Delivered enemy blueprint loads"), EnemyClass)) return false;
	const auto* Defaults = EnemyClass->GetDefaultObject<ACandyFightEnemyCharacter>();
	TestEqual(TEXT("Enemy capsule blocks the weapon channel"), Defaults->GetCapsuleComponent()->GetCollisionResponseToChannel(CandyFightWeapons::TraceChannel), ECR_Block);
	TestEqual(TEXT("Display body has no collision"), Defaults->Body->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Facing marker has no collision"), Defaults->FacingMarker->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Enemy capsule does not modify navigation"), Defaults->GetCapsuleComponent()->CanEverAffectNavigation());
	TestNotNull(TEXT("Enemy has shared combat component"), Defaults->FindComponentByClass<UCandyFightCombatComponent>());
	TestTrue(TEXT("Enemy uses external combat driver"), Defaults->Combat->IsExternallyDriven());
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Shooter = World->SpawnActor<AActor>();
	auto* Enemy = World->SpawnActor<ACandyFightEnemyCharacter>(EnemyClass, FVector(1000,0,88), FRotator::ZeroRotator);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightPatrolIntegration), false, Shooter);
	FHitResult Hit;
	World->LineTraceSingleByChannel(Hit, FVector(0,0,90), FVector(2000,0,90), CandyFightWeapons::TraceChannel, Params);
	TestTrue(TEXT("Bullet path stops on delivered enemy capsule"), Hit.bBlockingHit && Hit.GetActor() == Enemy);
	auto* Config = NewObject<UCandyFightWeaponConfig>();
	Config->ImpactEffect = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/CandyFight/VFX/Weapons/NS_WeaponImpact.NS_WeaponImpact"));
	TestNotNull(TEXT("Current project impact asset retained"), Config->ImpactEffect.Get());
	auto* Shot = World->SpawnActor<ACandyFightProjectileShot>();
	Shot->Initialize(*Config, FVector(0,0,90), {FVector::ForwardVector}, Shooter, nullptr, nullptr, nullptr, 0.);
	Shot->AdvanceTo(.3);
	TestTrue(TEXT("Projectile ends on enemy"), Shot->IsFinished());
	TestEqual(TEXT("Enemy impact requests existing particle effect once"), Shot->ImpactEffectRequests, 1);
    TestTrue(TEXT("Unattributed projectile cannot defeat an enemy"), IsValid(Enemy) && !Enemy->bDefeated);
    TestEqual(TEXT("Only player-attributed damage reduces enemy health"), Enemy->Health, 100.f);
	World->DestroyWorld(false);
	return true;
}
#endif
