#if WITH_DEV_AUTOMATION_TESTS
#include "CandyFight/CandyFightFireCameraComponent.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightTarget.h"
#include "CandyFight/UI/CandyFightCrosshairWidget.h"
#include "Components/BoxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFireCameraStateTest,"CandyFight.FireCamera.VectorAndReturn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightFireCameraStateTest::RunTest(const FString& Parameters)
{
    auto* Config=NewObject<UCandyFightWeaponConfig>();
    Config->WeaponClass=ACandyFightWeapon::StaticClass();
    FCandyFightFireCameraState State;
    FRandomStream Random(917);
    int32 BothAxes=0;
    for(uint64 Frame=0;Frame<1000;++Frame)
    {
        TestTrue(TEXT("Each new shot refreshes"),State.Trigger(*Config,Random,Frame*.01,Frame,FVector2D(-1,-1)));
        TestTrue(TEXT("Independent axis limits, no accumulated displacement"),FMath::Abs(State.Offset.X)<=4. && FMath::Abs(State.Offset.Y)<=4. && FMath::Abs(State.Roll)<=.300001);
        if(FMath::Abs(State.Offset.X)>.01 && FMath::Abs(State.Offset.Y)>.01) ++BothAxes;
    }
    TestTrue(TEXT("Both vector components are sampled simultaneously"),BothAxes>950);
    const FVector2D Initial=State.Offset;
    const double InitialRoll=State.Roll;
    const int32 Seed=Random.GetCurrentSeed();
    TestFalse(TEXT("Duplicate frame cannot resample"),State.Trigger(*Config,Random,9.99,999,FVector2D(-1,-1)));
    TestEqual(TEXT("Duplicate frame does not consume random numbers"),Random.GetCurrentSeed(),Seed);
    FVector2D Offset; double Roll;
    State.Evaluate(9.99,Offset,Roll);
    TestTrue(TEXT("Immediate full initial displacement"),Offset.Equals(Initial,1.e-9));
    State.Evaluate(10.115,Offset,Roll);
    TestTrue(TEXT("Half duration is half displacement"),Offset.Equals(Initial*.5,1.e-8));
    TestTrue(TEXT("Rotation uses same smoothstep weight"),FMath::IsNearlyEqual(Roll,InitialRoll*.5,1.e-8));
    const FVector2D Frozen=Offset;
    State.Evaluate(10.115,Offset,Roll);
    TestEqual(TEXT("Frozen game time does not decay"),Offset,Frozen);
    State.Evaluate(10.25,Offset,Roll);
    TestEqual(TEXT("Ends at exactly zero vector"),Offset,FVector2D::ZeroVector);
    TestEqual(TEXT("Ends at exactly zero angle"),Roll,0.);
    for(int32 FPS : {30,60,120})
    {
        State.Reset();Random.Initialize(27);State.Trigger(*Config,Random,0.,0,FVector2D(-1,-1));
        const FVector2D Start=State.Offset;
        for(int32 I=0;I<=FPS/10;++I) State.Evaluate(double(I)/FPS,Offset,Roll);
        TestTrue(TEXT("All frame rates agree at 0.1 game seconds"),Offset.Equals(Start*.648,1.e-7));
        State.Evaluate(.25,Offset,Roll);TestEqual(TEXT("All frame rates finish exactly"),Offset,FVector2D::ZeroVector);
    }
    State.Reset(); State.Trigger(*Config,Random,0.,1,FVector2D(-1,-1));const FVector2D Snapshot=State.Offset;
    Config->FireShakeReturnSeconds=2.f;Config->FireShakeMaxOffset=FVector2D(99,99);
    State.Evaluate(.125,Offset,Roll);TestTrue(TEXT("Config edits do not alter active snapshot"),Offset.Equals(Snapshot*.5,1.e-8));
    Config->bEnableFireCameraShake=false;State.Trigger(*Config,Random,.15,2,FVector2D(-1,-1));
    State.Evaluate(.15,Offset,Roll);TestEqual(TEXT("Disabled next shot yields zero"),Offset,FVector2D::ZeroVector);
    FMinimalViewInfo Base; Base.Location=FVector(100,200,300);Base.Rotation=FRotator(-55,73,0);Base.FOV=90.f;
    const auto Visual=FCandyFightFireCameraState::Compose(Base,FVector2D(3,-2),.3);
    const FQuat Basis=Base.Rotation.Quaternion();
    TestTrue(TEXT("Vector moves right and down at the same time"),Visual.Location.Equals(Base.Location+Basis.GetRightVector()*3.-Basis.GetUpVector()*2.,1.e-8));
    TestTrue(TEXT("Roll preserves view direction"),Visual.Rotation.Vector().Equals(Base.Rotation.Vector(),1.e-8));
    TestEqual(TEXT("No FOV change"),Visual.FOV,Base.FOV);
    FString Error;
    Config->FireShakeMaxOffset=FVector2D(-1,0);TestFalse(TEXT("Negative component rejected"),Config->ValidateConfiguration(Error));
    Config->FireShakeMaxOffset=FVector2D(0,std::numeric_limits<double>::infinity());TestFalse(TEXT("Infinite component rejected"),Config->ValidateConfiguration(Error));
    Config->FireShakeMaxOffset=FVector2D::ZeroVector;Config->FireShakeMaxRollDegrees=0;Config->FireShakeReturnSeconds=.25;
    TestTrue(TEXT("Zero strength allowed"),Config->ValidateConfiguration(Error));
    Config->FireShakeReturnSeconds=0;TestFalse(TEXT("Zero duration rejected"),Config->ValidateConfiguration(Error));
    Config->FireShakeReturnSeconds=.25;Config->FireShakeMaxRollDegrees=std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Invalid rotation rejected"),Config->ValidateConfiguration(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFireCameraRecoilTest,"CandyFight.FireCamera.RecoilDirection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightFireCameraRecoilTest::RunTest(const FString& Parameters)
{
    auto* Config=NewObject<UCandyFightWeaponConfig>();
    Config->WeaponClass=ACandyFightWeapon::StaticClass();
    Config->FireShakeMaxOffset=FVector2D(2,8); // Unequal limits must not distort the direction.
    FCandyFightFireCameraState State;
    FRandomStream Random(735);
    uint64 Frame=0;
    for(double CameraYaw : {0.,73.,180.})
    {
        const FQuat Basis=FRotator(-55,CameraYaw,0).Quaternion();
        for(int32 ShotYaw=0;ShotYaw<360;ShotYaw+=15)
        {
            const FVector Shot=FRotator(0,ShotYaw,0).Vector();
            const FVector2D Recoil=FCandyFightFireCameraState::ProjectRecoil(Shot,Basis);
            const FVector Projected=Basis.GetRightVector()*Recoil.X+Basis.GetUpVector()*Recoil.Y;
            TestTrue(TEXT("Recoil projection opposes world shot"),FVector::DotProduct(Projected,-Shot)>0.);
            for(int32 Sample=0;Sample<40;++Sample)
            {
                State.Trigger(*Config,Random,0.,Frame++,Recoil);
                TestTrue(TEXT("All quadrants stay inside recoil cone"),FVector2D::DotProduct(State.Offset.GetSafeNormal(),Recoil)>=FMath::Cos(FMath::DegreesToRadians(15.))-.000001);
                TestTrue(TEXT("Asymmetric per-axis limits retained"),FMath::Abs(State.Offset.X)<=2.000001 && FMath::Abs(State.Offset.Y)<=8.000001);
                TestTrue(TEXT("Recoil is not zero"),State.Offset.Size()>.1);
            }
        }
    }
    Config->FireShakeDirectionJitterDegrees=0;
    State.Trigger(*Config,Random,0.,Frame++,FVector2D(1,-1));
    TestTrue(TEXT("Zero jitter is exactly reverse projected direction"),State.Offset.GetSafeNormal().Equals(FVector2D(1,-1).GetSafeNormal(),1.e-8));
    Config->FireShakeMaxOffset=FVector2D(0,8);
    State.Trigger(*Config,Random,0.,Frame++,FVector2D(0,-1));
    TestTrue(TEXT("Axis aligned recoil works with other axis disabled"),State.Offset.X==0 && State.Offset.Y<0);
    State.Trigger(*Config,Random,0.,Frame++,FVector2D(1,-1));
    TestTrue(TEXT("Impossible direction does not turn into sideways recoil"),State.Offset.IsNearlyZero());
    Config->FireShakeMaxOffset=FVector2D::ZeroVector;
    State.Trigger(*Config,Random,0.,Frame++,FVector2D(1,-1));
    TestEqual(TEXT("Zero translation strength"),State.Offset,FVector2D::ZeroVector);
    const FVector2D Degenerate=FCandyFightFireCameraState::ProjectRecoil(FVector::ForwardVector,FQuat::Identity);
    State.Trigger(*Config,Random,0.,Frame++,Degenerate);
    TestEqual(TEXT("View parallel shot has no arbitrary translation"),State.Offset,FVector2D::ZeroVector);
    FString Error;
    for(float Invalid : {-1.f,46.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
    {
        Config->FireShakeDirectionJitterDegrees=Invalid;
        TestFalse(TEXT("Invalid direction jitter rejected"),Config->ValidateConfiguration(Error));
    }
    Config->FireShakeDirectionJitterDegrees=0;
    TestTrue(TEXT("Zero jitter valid"),Config->ValidateConfiguration(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightFireCameraIntegrationTest,"CandyFight.FireCamera.CombatAndFinalCamera",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCandyFightFireCameraIntegrationTest::RunTest(const FString& Parameters)
{
    // Transient test world only, never editor Play.
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Character=World->SpawnActor<ACandyFightCharacter>();
    auto* PC=World->SpawnActor<APlayerController>();
    PC->Possess(Character);
    PC->Player=NewObject<ULocalPlayer>(GEngine); // Standalone local ownership without creating a viewport or PIE.
    PC->SetViewTarget(Character);
    if(!PC->PlayerCameraManager) PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>();
    auto* Manager=PC->PlayerCameraManager.Get();
    Manager->InitializeFor(PC);
    Manager->SetViewTarget(Character);
    TestTrue(TEXT("Fixture is a local controller"),PC->IsLocalController());
    TestTrue(TEXT("Fixture possesses character"),PC->GetPawn()==Character);
    auto* Combat=Character->Combat.Get();auto* Feedback=Character->FireCameraFeedback.Get();
    auto* Config=NewObject<UCandyFightWeaponConfig>();Config->WeaponClass=ACandyFightWeapon::StaticClass();
    Config->FireMode=ECandyFightFireMode::Shotgun;Config->PelletCount=12;Config->Damage=8;Config->BaseSpread=Config->MaxSpread=8;
    Combat->WeaponConfigs={Config,Config};Combat->WeaponStates.SetNum(2);
    for(auto& State:Combat->WeaponStates) State.Initialize(*Config,0.);
    TestTrue(TEXT("Equip shotgun"),Combat->EquipTestWeapon(1));
    Character->AimWorldLocation=FVector(2000,0,20);Character->AimScreenPosition=FVector2D(600,400);
    Character->bHasValidAim=true;
    Character->CurrentAimCameraOffset=FVector(200,100,0);
    Character->CrosshairWidget=NewObject<UCandyFightCrosshairWidget>(Character);
    Character->CrosshairWidget->ScreenPosition=Character->AimScreenPosition;
    const FVector Aim=Character->AimWorldLocation;
    const FRotator ActorRotation=Character->GetActorRotation();
    const FRotator ControlRotation=PC->GetControlRotation();
    auto* Target=World->SpawnActor<ACandyFightTarget>(FVector(1000,0,20),FRotator::ZeroRotator);
    Target->HitBox->SetBoxExtent(FVector(40,400,200));
    FMinimalViewInfo Base;Base.Location=FVector(-1200,0,1600);Base.Rotation=FRotator(-55,0,0);Base.FOV=90;
    Manager->SetCameraCachePOV(Base);Feedback->RecordLogicalCamera(GFrameCounter,Base.Rotation);
    Combat->Random.Initialize(123);
    const int32 AmmoBefore=Combat->GetAmmo();
    Combat->FireShot(*Character,0.);
    const auto Ends=Combat->LastShotEnds;
    TestEqual(TEXT("Shotgun triggers one shake for twelve pellets"),Feedback->ShakeRequests,1);
    TestEqual(TEXT("Shake does not change ammo consumption"),Combat->GetAmmo(),AmmoBefore-1);
    TestEqual(TEXT("One fire sound"),Combat->FireSoundRequests,1);
    Feedback->ApplyVisualCamera(0.,GFrameCounter);
    const FMinimalViewInfo Visual=Manager->GetCameraCacheView();
    TestFalse(TEXT("First shot affects same frame final camera"),Visual.Location.Equals(Base.Location,1.e-6));
    TestTrue(TEXT("Both components visibly nonzero"),FMath::Abs(Feedback->CurrentOffset.X)>1.e-6 && FMath::Abs(Feedback->CurrentOffset.Y)>1.e-6);
    const FVector ShotDirection=(Aim-Combat->EquippedWeapon->Muzzle->GetComponentLocation()).GetSafeNormal2D();
    TestTrue(TEXT("Actual shot camera translation follows recoil"),FVector::DotProduct(Visual.Location-Base.Location,-ShotDirection)>0.);
    Feedback->ApplyVisualCamera(0.,GFrameCounter);
    TestEqual(TEXT("Repeated frame does not accumulate location"),Manager->GetCameraCacheView().Location,Visual.Location);
    TestEqual(TEXT("Logic aim untouched"),Character->AimWorldLocation,Aim);
    TestEqual(TEXT("Character yaw untouched"),Character->GetActorRotation(),ActorRotation);
    TestEqual(TEXT("Controller rotation untouched"),PC->GetControlRotation(),ControlRotation);
    TestEqual(TEXT("Crosshair stays at same screen coordinate"),Character->CrosshairWidget->ScreenPosition,FVector2D(600,400));
    TestEqual(TEXT("Aim framing untouched"),Character->CurrentAimCameraOffset,FVector(200,100,0));
    TestTrue(TEXT("Paused-camera reevaluation is enabled"),Feedback->PrimaryComponentTick.bTickEvenWhenPaused);
    Combat->ActiveShots.Last()->AdvanceTo(.25);
    TestEqual(TEXT("Shake-enabled twelve pellets still deal 96"),Target->LastDamage,96.f);
    Target->Health=200;
    Combat->StopFire();Combat->Reload();Combat->ReleaseFireIntent();
    TestEqual(TEXT("Reload/release/focus creates no shake"),Feedback->ShakeRequests,1);
    Feedback->ApplyVisualCamera(.125,GFrameCounter);
    TestTrue(TEXT("Release still decays naturally"),(Manager->GetCameraCacheView().Location-Base.Location).Equals((Visual.Location-Base.Location)*.5,1.e-7));
    Feedback->ApplyVisualCamera(.25,GFrameCounter);
    TestEqual(TEXT("Final camera returns exactly to base"),Manager->GetCameraCacheView().Location,Base.Location);
    TestEqual(TEXT("Final rotation returns exactly to base"),Manager->GetCameraCacheView().Rotation,Base.Rotation);
    Feedback->ClearFeedback();Config->bEnableFireCameraShake=false;
    Combat->Random.Initialize(123);Combat->WeaponStates[0].Initialize(*Config,0.);
    Combat->FireShot(*Character,1.);
    TestEqual(TEXT("Visual random stream never changes ballistics"),Combat->LastShotEnds,Ends);
    TestEqual(TEXT("Disabled setting does not request shake"),Feedback->ShakeRequests,1);
    Combat->ActiveShots.Last()->AdvanceTo(1.25);
    TestEqual(TEXT("Shake-disabled damage also equals 96"),Target->Health,104.f);
    Config->bEnableFireCameraShake=true;Feedback->ClearFeedback();Feedback->NotifyShot(*Config,2.,FVector::ForwardVector);
    Feedback->ApplyVisualCamera(2.,GFrameCounter);
    const FVector BeforeEquip=Manager->GetCameraCacheView().Location;
    TestTrue(TEXT("Repeat current equip succeeds"),Combat->EquipTestWeapon(1));
    TestFalse(TEXT("Invalid equip fails"),Combat->EquipTestWeapon(99));
    TestEqual(TEXT("Repeat/invalid equip preserve visual state"),Manager->GetCameraCacheView().Location,BeforeEquip);
    TestTrue(TEXT("Actual switch succeeds"),Combat->EquipTestWeapon(2));
    TestEqual(TEXT("Actual switch restores base immediately"),Manager->GetCameraCacheView().Location,Base.Location);
    TestFalse(TEXT("Switch clears state"),Feedback->State.bActive);
    Feedback->NotifyShot(*Config,3.,FVector::ForwardVector);Feedback->ApplyVisualCamera(3.,GFrameCounter);
    PC->UnPossess();TestFalse(TEXT("Unpossess clears without waiting for tick"),Feedback->State.bActive);
    PC->Possess(Character);PC->SetViewTarget(Character);Manager->SetCameraCachePOV(Base);
    Feedback->RecordLogicalCamera(GFrameCounter,Base.Rotation);Feedback->NotifyShot(*Config,4.,FVector::ForwardVector);
    PC->SetViewTarget(Target);Feedback->ApplyVisualCamera(4.,GFrameCounter);
    TestFalse(TEXT("View target change clears old shake"),Feedback->State.bActive);
    PC->SetViewTarget(Character);Manager->SetCameraCachePOV(Base);Feedback->RecordLogicalCamera(GFrameCounter,Base.Rotation);
    Feedback->NotifyShot(*Config,5.,FVector::ForwardVector);Feedback->ApplyVisualCamera(5.,GFrameCounter);
    Feedback->DestroyComponent();TestEqual(TEXT("Destroying component restores cached camera"),Manager->GetCameraCacheView().Location,Base.Location);
    Combat->ClearProjectiles();World->DestroyWorld(false);
    return true;
}
#endif
