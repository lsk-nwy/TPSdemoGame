#include "CandyFightEnemyWeaponComponent.h"
#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyAIController.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UCandyFightEnemyWeaponComponent::UCandyFightEnemyWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}
void UCandyFightEnemyWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner()))
    {
        Enemy->Combat->AddTickPrerequisiteComponent(this);
        AddTickPrerequisiteComponent(Enemy->GetCharacterMovement());
        Enemy->Combat->OnShot.AddUniqueDynamic(this, &ThisClass::HandleShot);
    }
}
bool UCandyFightEnemyWeaponComponent::StartTest(FString& Error)
{
    if (bTesting) return true;
    auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner());
    auto* AI = Enemy ? Cast<ACandyFightEnemyAIController>(Enemy->GetController()) : nullptr;
    auto* Config = Enemy ? Enemy->Combat->GetCurrentConfig() : nullptr;
    if (!Enemy || !AI || !IsValid(Enemy->TestTarget) || Enemy->TestTarget == Enemy)
        { Error = TEXT("Missing controller or valid TestTarget"); return false; }
    if (!Enemy->EnemyConfig || !Enemy->EnemyConfig->IsValidWeaponDriver())
        { Error = TEXT("Invalid BurstShotCount / BurstPauseSeconds"); return false; }
    if (!Config || !Config->ValidateConfiguration(Error) || !IsValid(Enemy->Combat->EquippedWeapon) || !Enemy->Combat->EquippedWeapon->ValidateMuzzle())
        { Error = TEXT("Invalid equipped weapon or muzzle: ") + Error; return false; }
    if (!AI->BeginManualTest()) { Error = TEXT("Could not suspend patrol"); return false; }
    bTesting = true;
    Enemy->Combat->ClearExternalFireRequest();
    return true;
}
void UCandyFightEnemyWeaponComponent::StopTest(bool bResumePatrol)
{
    if (!bTesting && bResumePatrol) return; // Stop 命令仅结束手动测试，不打断自主射击。
    const bool bWasTesting = bTesting;
    InterruptAutonomousBurst();
    bTesting = false; bValidAim = false; Burst.Stop();
    if (auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner()))
    {
        Enemy->Combat->ClearExternalFireRequest();
        if (bWasTesting && bResumePatrol)
            if (auto* AI = Cast<ACandyFightEnemyAIController>(Enemy->GetController())) AI->EndManualTest();
    }
}
void UCandyFightEnemyWeaponComponent::UpdateDriver(double Now)
{
    if (UGameplayStatics::IsGamePaused(this)) return;
    auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner());
    if (!Enemy) return;
    Enemy->FacingMarker->SetVisibility(!IsValid(Enemy->Combat->EquippedWeapon));
    bValidAim = false;
    auto* AI = Cast<ACandyFightEnemyAIController>(Enemy->GetController());
    AActor* Target = bTesting ? Enemy->TestTarget.Get() : AI ? AI->GetVisibleCombatTarget() : nullptr;
    if (!bTesting && !Target) { InterruptAutonomousBurst(); return; }
    if (!Enemy->GetController() || !IsValid(Target) || !Enemy->EnemyConfig || !Enemy->EnemyConfig->IsValidWeaponDriver())
        { InterruptAutonomousBurst(); StopTest(); return; }
    auto* Combat = Enemy->Combat.Get();
    auto* Config = Combat->GetCurrentConfig();
    if (!Config || !IsValid(Combat->EquippedWeapon) || !Combat->EquippedWeapon->ValidateMuzzle()) { InterruptAutonomousBurst(); StopTest(); return; }
    // 先完成本帧朝向和目标更新，再让 Combat 消费请求；不用玩家鼠标或相机。
    AimLocation = Target->GetActorLocation();
    AimLocation.Z = Combat->GetAimPlaneZ(Enemy->GetActorLocation().Z);
    const FVector Direction = (AimLocation - Enemy->GetActorLocation()).GetSafeNormal2D();
    if (AimLocation.ContainsNaN() || Direction.IsNearlyZero()) { Combat->ClearExternalFireRequest(); return; }
    Enemy->SetActorRotation(FRotator(0., Direction.Rotation().Yaw, 0.));
    Combat->EquippedWeapon->AimAt(AimLocation, Enemy->GetActorForwardVector());
    AimFrame = GFrameCounter; bValidAim = true;
    if (!bTesting && (!AI || !AI->CanAutonomousFire())) { InterruptAutonomousBurst(); return; }
    // 每帧先释放，半自动也能明确地产生一次新按下；成功发射才计数。
    Combat->ClearExternalFireRequest();
    if (Now + 1.e-6 < Burst.NextGroupTime || Combat->IsReloading()) return;
    if (Combat->GetAmmo() <= 0) { Combat->Reload(); return; }
    Combat->StartFire();
}
void UCandyFightEnemyWeaponComponent::HandleShot(int32 WeaponNumber, int32 Pellets)
{
    auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner());
    if (!Enemy || !Enemy->EnemyConfig) return;
    Burst.CommitShot(Enemy->EnemyConfig->BurstShotCount, Enemy->EnemyConfig->BurstPauseSeconds,
        Enemy->Combat->GetAmmo() == 0, GetWorld()->GetTimeSeconds());
    Enemy->Combat->ClearExternalFireRequest();
}
void UCandyFightEnemyWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    UpdateDriver(GetWorld()->GetTimeSeconds());
}
void UCandyFightEnemyWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopTest(false);
    if (auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner())) Enemy->Combat->OnShot.RemoveDynamic(this, &ThisClass::HandleShot);
    Super::EndPlay(Reason);
}


void UCandyFightEnemyWeaponComponent::InterruptAutonomousBurst()
{
    auto* Enemy = Cast<ACandyFightEnemyCharacter>(GetOwner());
    if (!Enemy) return;
    Burst.Interrupt(Enemy->EnemyConfig ? Enemy->EnemyConfig->BurstPauseSeconds : 1.);
    Enemy->Combat->ClearExternalFireRequest();
}
