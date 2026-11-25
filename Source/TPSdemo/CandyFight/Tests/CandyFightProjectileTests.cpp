#if WITH_DEV_AUTOMATION_TESTS
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/CandyFightCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/LineBatchComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Misc/App.h"
#include <limits>

namespace
{
    struct FProjectileWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        AActor* Shooter = World->SpawnActor<AActor>();
        UCandyFightWeaponConfig* Config = NewObject<UCandyFightWeaponConfig>();
        FProjectileWorld() { Config->WeaponClass = ACandyFightWeapon::StaticClass(); }
        ~FProjectileWorld() { World->DestroyWorld(false); }
        ACandyFightProjectileShot* Shot(const TArray<FVector>& Directions = {FVector::ForwardVector})
        {
            auto* Shot = World->SpawnActor<ACandyFightProjectileShot>();
            Shot->Initialize(*Config,FVector(0,0,90),Directions,Shooter,nullptr,nullptr,nullptr,0.);
            return Shot;
        }
        ACandyFightTarget* Target(FVector Position, FVector Extent = FVector(40,40,90))
        {
            auto* Target = World->SpawnActor<ACandyFightTarget>(Position,FRotator::ZeroRotator);
            Target->HitBox->SetBoxExtent(Extent);
            return Target;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightProjectileFlightTest,"CandyFight.Projectiles.FlightAndCollision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightProjectileFlightTest::RunTest(const FString& Parameters)
{
    for(int32 FPS : {30,60,120})
    {
        FProjectileWorld Env;
        auto* Target=Env.Target(FVector(1040,0,90));
        auto* Shot=Env.Shot();
        Shot->AdvanceTo(0.);
        TestEqual(TEXT("Spawn frame does not pre-advance"),Shot->GetPellets()[0].Position,FVector(0,0,90));
        TestEqual(TEXT("No instant damage"),Target->Health,200.f);
        int32 Frame=0;
        while(!Shot->IsFinished() && Frame<FPS)
        {
            ++Frame;
            Shot->AdvanceTo(double(Frame)/FPS);
            if(double(Frame)/FPS < .2-1.e-6) TestEqual(TEXT("No damage before arrival"),Target->Health,200.f);
        }
        TestTrue(TEXT("Arrival time within one frame"),FMath::Abs(double(Frame)/FPS-.2)<=1./FPS+1.e-6);
        TestEqual(TEXT("One hit after flight"),Target->Health,180.f);
        TestTrue(TEXT("Finished tracer is cleared"),Shot->Tracers->BatchedLines.IsEmpty());
    }
    {
        FProjectileWorld Env;
        auto* Wall=Env.Target(FVector(1000,0,90),FVector(1,40,90));
        auto* Behind=Env.Target(FVector(1400,0,90));
        Env.Config->ProjectileSpeed=100000.f;
        auto* Shot=Env.Shot(); Shot->AdvanceTo(.05);
        TestEqual(TEXT("High speed/hitch hits thin first wall"),Wall->Health,180.f);
        TestEqual(TEXT("No penetration to target behind wall"),Behind->Health,200.f);
    }
    {
        FProjectileWorld Env;
        auto* Target=Env.Target(FVector(1040,500,90));
        auto* Shot=Env.Shot(); Shot->AdvanceTo(.1);
        Target->SetActorLocation(FVector(1040,0,90)); Shot->AdvanceTo(.21);
        TestEqual(TEXT("Target entering live path is hit"),Target->Health,180.f);
        Target->Health=200.f;
        auto* Dodge=Env.Shot(); Dodge->AdvanceTo(.1);
        Target->SetActorLocation(FVector(1040,500,90)); Dodge->AdvanceTo(.21);
        TestEqual(TEXT("Target leaving live path dodges"),Target->Health,200.f);
    }
    {
        FProjectileWorld Env;
        auto* Beyond=Env.Target(FVector(5100,0,90));
        auto* Shot=Env.Shot(); Shot->AdvanceTo(.001);
        TestEqual(TEXT("Tracer drawn for moving pellet"),Shot->Tracers->BatchedLines.Num(),1);
        TestTrue(TEXT("Tracer never longer than traveled distance"),(Shot->Tracers->BatchedLines[0].End-Shot->Tracers->BatchedLines[0].Start).Size()<=5.001);
        Shot->AdvanceTo(.1);
        TestTrue(TEXT("Tracer length capped"),(Shot->Tracers->BatchedLines[0].End-Shot->Tracers->BatchedLines[0].Start).Size()<=30.001);
        const FVector Paused=Shot->GetPellets()[0].Position;
        Shot->AdvanceTo(.1); Shot->AdvanceTo(.1);
        TestEqual(TEXT("Frozen game time freezes projectile"),Shot->GetPellets()[0].Position,Paused);
        Env.Config->ProjectileSpeed=1.f; Env.Config->Range=1.f; Env.Config->Damage=99.f;
        Shot->AdvanceTo(.2);
        TestTrue(TEXT("Launch snapshot ignores edited speed/range"),Shot->GetPellets()[0].Position.Equals(FVector(1000,0,90),.001));
        Shot->AdvanceTo(2.);
        TestTrue(TEXT("Range expires shot"),Shot->IsFinished());
        TestEqual(TEXT("Beyond-range target unharmed"),Beyond->Health,200.f);
        Env.Config->TracerLength=0.f; Env.Config->Range=5000; Env.Config->ProjectileSpeed=5000;
        auto* Hidden=Env.Shot(); Hidden->AdvanceTo(.1);
        TestTrue(TEXT("Hidden tracer still moves"),Hidden->GetPellets()[0].Position.Equals(FVector(500,0,90),.001));
        TestTrue(TEXT("Length zero renders no lines"),Hidden->Tracers->BatchedLines.IsEmpty());
        Env.Shooter->Destroy(); Hidden->AdvanceTo(.2);
        TestTrue(TEXT("Destroyed shooter ends flight"),Hidden->IsFinished());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightProjectileDamageTest,"CandyFight.Projectiles.ArrivalDamage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightProjectileDamageTest::RunTest(const FString& Parameters)
{
    FProjectileWorld Env;
    Env.Config->Damage=8.f;
    auto* Target=Env.Target(FVector(1040,0,90),FVector(40,1000,90));
    TArray<FVector> Directions;
    for(int32 I=0;I<12;++I) Directions.Add(FVector::ForwardVector.RotateAngleAxis(I<6?0.:8.,FVector::UpVector));
    auto* Shot=Env.Shot(Directions);
    Shot->AdvanceTo(.20001);
    TestEqual(TEXT("First six immediately apply 48 damage"),Target->Health,152.f);
    TestEqual(TEXT("Same-frame same-target damage is merged"),Target->HitCount,1);
    Shot->AdvanceTo(.21);
    TestEqual(TEXT("Later six immediately apply remaining 48"),Target->Health,104.f);
    TestEqual(TEXT("Different frame has another damage event"),Target->HitCount,2);
    TestEqual(TEXT("Shot display accumulates 96"),Target->LastDamage,96.f);
    auto* Older=Env.Shot();
    auto* Newer=Env.Shot();
    Newer->AdvanceTo(.21);
    TestEqual(TEXT("New shot replaces display"),Target->LastDamage,8.f);
    FCandyFightProjectileDamageEvent OldEvent; OldEvent.ShotId=Older->GetShotId(); OldEvent.CumulativeDamage=64.f;
    Target->TakeDamage(8.f,OldEvent,nullptr,Older);
    TestEqual(TEXT("Old delayed shot still damages"),Target->Health,88.f);
    TestEqual(TEXT("Old delayed shot cannot overwrite new display"),Target->LastDamage,8.f);
    Target->Health=4.f; Newer=Env.Shot(); Newer->AdvanceTo(.21);
    TestEqual(TEXT("Lethal arrival disables target"),Target->Health,0.f);
    const int32 Hits=Target->HitCount;
    Target->TakeDamage(8.f,OldEvent,nullptr,Older);
    TestEqual(TEXT("Dead target rejects later damage"),Target->HitCount,Hits);
    FString Error;
    for(float Speed : {0.f,-1.f,std::numeric_limits<float>::infinity()})
    { Env.Config->ProjectileSpeed=Speed; TestFalse(TEXT("Invalid projectile speed rejected"),Env.Config->ValidateConfiguration(Error)); }
    Env.Config->ProjectileSpeed=5000.f;
    Env.Config->TracerLength=-1.f; TestFalse(TEXT("Negative tracer length rejected"),Env.Config->ValidateConfiguration(Error));
    Env.Config->TracerLength=0.f; TestTrue(TEXT("Invisible projectile is valid"),Env.Config->ValidateConfiguration(Error));
    Env.Config->TracerThickness=0.f; TestFalse(TEXT("Zero width rejected"),Env.Config->ValidateConfiguration(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightProjectileCombatTest,"CandyFight.Projectiles.CombatLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightProjectileCombatTest::RunTest(const FString& Parameters)
{
    FProjectileWorld Env;
    auto* Character=Env.World->SpawnActor<ACandyFightCharacter>();
    auto* Combat=Character->Combat.Get();
    Combat->WeaponConfigs={Env.Config,Env.Config}; Combat->WeaponStates.SetNum(2);
    for(auto& State:Combat->WeaponStates) State.Initialize(*Env.Config,0.);
    TestTrue(TEXT("Equip"),Combat->EquipTestWeapon(1));
    Character->AimWorldLocation=FVector(2000,0,20);
    Character->bHasValidAim=true;
    Combat->FireShot(*Character,0.);
    auto* Shot=Combat->ActiveShots.Last().Get();
    TestTrue(TEXT("Switch weapon"),Combat->EquipTestWeapon(2));
    Combat->Reload(); Combat->StopFire(); Combat->ReleaseFireIntent();
    Shot->AdvanceTo(.1);
    TestFalse(TEXT("Switch/reload/release do not cancel flight"),Shot->IsFinished());
    TestTrue(TEXT("Flight continues after gun destroyed by switch"),Shot->GetPellets()[0].Distance>499.);
    Combat->ClearProjectiles();
    TestTrue(TEXT("Explicit cleanup destroys projectile"),!IsValid(Shot));
    TestTrue(TEXT("Combat owns no remaining shots"),Combat->ActiveShots.IsEmpty());
    auto* Blocker=Env.Target(FVector(15,24,20),FVector(20,30,100));
    Combat->FireShot(*Character,.2);
    TestTrue(TEXT("Muzzle obstruction finishes shot immediately"),!Combat->ActiveShots.Last().IsValid());
    TestTrue(TEXT("Obstruction still commits ammo"),Combat->GetAmmo()<Env.Config->MagazineCapacity);
    TestTrue(TEXT("Muzzle obstruction receives damage"),Blocker->Health<200.f);
    Blocker->Destroy();
    Combat->FireShot(*Character,.4);
    Shot=Combat->ActiveShots.Last().Get();
    Combat->DestroyComponent();
    TestTrue(TEXT("Component destruction clears flight even without EndPlay"),!IsValid(Shot));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightProjectileImpactTest,"CandyFight.Projectiles.ImpactVFX",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightProjectileImpactTest::RunTest(const FString& Parameters)
{
    auto* Effect=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/CandyFight/VFX/Weapons/NS_WeaponImpact.NS_WeaponImpact"));
    if(!TestNotNull(TEXT("Migrated effect loads"),Effect)) return false;
    for(int32 FPS : {30,60,120})
    {
        FProjectileWorld Env;
        Env.Config->ImpactEffect=Effect;
        Env.Config->ImpactEffectScale=2.f;
        auto* Target=Env.Target(FVector(1040,0,90));
        auto* Shot=Env.Shot();
        Env.Config->ImpactEffect=nullptr;Env.Config->ImpactEffectScale=4.f;
        TestEqual(TEXT("Fire does not request a premature impact"),Shot->ImpactEffectRequests,0);
        int32 Frame=0;
        while(!Shot->IsFinished() && Frame<FPS)
        {
            Shot->AdvanceTo(double(++Frame)/FPS);
            if(double(Frame)/FPS<.2-1.e-6) TestEqual(TEXT("No effect before collision"),Shot->ImpactEffectRequests,0);
        }
        TestEqual(TEXT("First collision requests one effect"),Shot->ImpactEffectRequests,1);
        TestTrue(TEXT("One cm outward from surface"),Shot->LastImpactTransform.GetLocation().Equals(FVector(999,0,90),.001));
        TestTrue(TEXT("Local X faces surface normal"),Shot->LastImpactTransform.GetRotation().GetForwardVector().Equals(-FVector::ForwardVector,.001));
        TestEqual(TEXT("Scale saved at launch"),Shot->LastImpactTransform.GetScale3D(),FVector(2));
        TestEqual(TEXT("System reference saved at launch"),Shot->ImpactEffect.Get(),Effect);
        TestEqual(TEXT("Damage unchanged"),Target->Health,180.f);
        Shot->AdvanceTo(1.);
        TestEqual(TEXT("Finished shot cannot repeat impact"),Shot->ImpactEffectRequests,1);
        if(FApp::CanEverRender()) TestNotNull(TEXT("Renderer creates a real Niagara component"),Shot->LastImpactComponent.Get());
        if(auto* Component=Shot->LastImpactComponent.Get())
        {
            TestTrue(TEXT("Effect outlives finished shot"),IsValid(Component));
            TestTrue(TEXT("Effect owner is not shot"),Component->GetOwner()!=Shot);
            TestNull(TEXT("Impact is not attached"),Component->GetAttachParent());
            TestFalse(TEXT("Niagara component does not tick while paused"),Component->PrimaryComponentTick.bTickEvenWhenPaused);
            Component->AdvanceSimulation(600,1.f/60.f);
            TestTrue(TEXT("Template burst completes and unregisters automatically"),!IsValid(Component) || !Component->IsRegistered());
        }
        else AddInfo(TEXT("No renderer: verified impact request; rendered component lifetime requires RHI test or user acceptance."));
    }
    {
        FProjectileWorld Env;Env.Config->ImpactEffect=Effect;Env.Config->Damage=8;
        auto* Target=Env.Target(FVector(1040,0,90),FVector(40,1000,90));
        TArray<FVector> Directions;
        for(int32 I=0;I<12;++I) Directions.Add(FVector::ForwardVector.RotateAngleAxis(I<6?0.:8.,FVector::UpVector));
        auto* Shot=Env.Shot(Directions);Shot->AdvanceTo(.20001);
        TestEqual(TEXT("First six arrivals each request effect"),Shot->ImpactEffectRequests,6);
        Shot->AdvanceTo(.21);
        TestEqual(TEXT("Twelve arrivals request twelve effects"),Shot->ImpactEffectRequests,12);
        TestEqual(TEXT("Damage remains 96 total"),Target->LastDamage,96.f);
        TestEqual(TEXT("Same-frame damage stays merged"),Target->HitCount,2);
        Shot=Env.Shot(Directions);Shot->AdvanceTo(.3);
        TestEqual(TEXT("Same-frame twelve hits still request twelve effects"),Shot->ImpactEffectRequests,12);
        TestEqual(TEXT("Same-frame twelve damage hits only once"),Target->HitCount,3);
    }
    {
        FProjectileWorld Env;Env.Config->ImpactEffect=Effect;
        auto* Wall=Env.World->SpawnActor<AActor>();
        auto* Box=NewObject<UBoxComponent>(Wall);
        Wall->SetRootComponent(Box);Box->SetBoxExtent(FVector(1,100,100));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();Wall->SetActorLocation(FVector(1000,0,90));
        auto* Behind=Env.Target(FVector(1400,0,90));Env.Config->ProjectileSpeed=100000;
        auto* Shot=Env.Shot();Shot->AdvanceTo(.1);
        TestEqual(TEXT("Non-damageable thin wall requests effect at high speed"),Shot->ImpactEffectRequests,1);
        TestEqual(TEXT("No damage through wall"),Behind->Health,200.f);
        Env.Config->ImpactEffect=nullptr;Shot=Env.Shot();Shot->AdvanceTo(.1);
        TestEqual(TEXT("Missing asset suppresses visual only"),Shot->ImpactEffectRequests,0);
        TestTrue(TEXT("Missing asset still stops at wall"),Shot->IsFinished());
        Wall->Destroy();Env.Config->Range=500;Env.Config->ImpactEffect=Effect;
        Shot=Env.Shot();Shot->AdvanceTo(.1);
        TestEqual(TEXT("Range end has no impact"),Shot->ImpactEffectRequests,0);
    }
    {
        FProjectileWorld Env;Env.Config->ImpactEffect=Effect;
        auto* Target=Env.Target(FVector(50,0,90));
        FHitResult Hit(Target,Target->HitBox,FVector(10,0,90),-FVector::ForwardVector);
        Hit.bBlockingHit=true;
        auto* Shot=Env.Shot();Shot->ResolveMuzzleObstruction(Hit);
        TestEqual(TEXT("Valid muzzle hit requests effect"),Shot->ImpactEffectRequests,1);
        TestEqual(TEXT("Muzzle damage retained"),Target->Health,180.f);
        Shot->ResolveMuzzleObstruction(Hit);
        TestEqual(TEXT("Repeated resolution does not replay"),Shot->ImpactEffectRequests,1);
        Shot=Env.Shot();Shot->ResolveMuzzleObstruction(FHitResult());
        TestEqual(TEXT("Overlap-only never fakes an impact point"),Shot->ImpactEffectRequests,0);
        Hit.bStartPenetrating=true;Shot=Env.Shot();Shot->ResolveMuzzleObstruction(Hit);
        TestEqual(TEXT("Penetrating start has no reliable surface effect"),Shot->ImpactEffectRequests,0);
        FString Error;
        for(float Scale : {0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        {
            Env.Config->ImpactEffectScale=Scale;
            TestFalse(TEXT("Invalid effect scale rejected"),Env.Config->ValidateConfiguration(Error));
        }
    }
    return true;
}
#endif
