#include "CandyFight/CandyFightFireCameraComponent.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void FCandyFightFireCameraState::Reset()
{
    Offset = FVector2D::ZeroVector;
    Roll = 0.;
    bActive = false;
    LastShotFrame = MAX_uint64;
}

FVector2D FCandyFightFireCameraState::ProjectRecoil(const FVector& ShotDirection, const FQuat& CameraRotation)
{
    if (ShotDirection.ContainsNaN() || CameraRotation.ContainsNaN()) return FVector2D::ZeroVector;
    return FVector2D(FVector::DotProduct(-ShotDirection, CameraRotation.GetRightVector()),
        FVector::DotProduct(-ShotDirection, CameraRotation.GetUpVector())).GetSafeNormal();
}

bool FCandyFightFireCameraState::Trigger(const UCandyFightWeaponConfig& Config, FRandomStream& Random, double Now, uint64 Frame, const FVector2D& RecoilDirection)
{
    if (LastShotFrame == Frame) return false;
    LastShotFrame = Frame;
    if (!Config.bEnableFireCameraShake)
    {
        Offset = FVector2D::ZeroVector; Roll = 0.; bActive = false;
        return false;
    }
    // Rotate a single recoil vector, then scale uniformly to fit both axis limits.
    // Independent axis scaling would distort its direction with asymmetric settings.
    const double Angle = FMath::DegreesToRadians(Random.FRandRange(-Config.FireShakeDirectionJitterDegrees, Config.FireShakeDirectionJitterDegrees));
    const double Strength = Random.FRandRange(.6f, 1.f);
    Offset = FVector2D::ZeroVector;
    if (!RecoilDirection.ContainsNaN() && !RecoilDirection.IsNearlyZero())
    {
        const FVector2D Unit = RecoilDirection.GetSafeNormal();
        const FVector2D Direction(Unit.X * FMath::Cos(Angle) - Unit.Y * FMath::Sin(Angle),
            Unit.X * FMath::Sin(Angle) + Unit.Y * FMath::Cos(Angle));
        double Limit = TNumericLimits<double>::Max();
        if (FMath::Abs(Direction.X) > UE_DOUBLE_SMALL_NUMBER) Limit = FMath::Min(Limit, Config.FireShakeMaxOffset.X / FMath::Abs(Direction.X));
        if (FMath::Abs(Direction.Y) > UE_DOUBLE_SMALL_NUMBER) Limit = FMath::Min(Limit, Config.FireShakeMaxOffset.Y / FMath::Abs(Direction.Y));
        Offset = Direction * (Limit * Strength);
    }
    Roll = Random.FRandRange(-Config.FireShakeMaxRollDegrees, Config.FireShakeMaxRollDegrees);
    StartedAt = Now;
    Duration = Config.FireShakeReturnSeconds;
    bActive = true;
    return true;
}

void FCandyFightFireCameraState::Evaluate(double Now, FVector2D& OutOffset, double& OutRoll)
{
    OutOffset = FVector2D::ZeroVector;
    OutRoll = 0.;
    if (!bActive) return;
    const double T = FMath::Clamp((Now - StartedAt) / Duration, 0., 1.);
    if (T >= 1.) { Offset = FVector2D::ZeroVector; Roll = 0.; bActive = false; return; }
    const double Weight = 1. - T * T * (3. - 2. * T);
    OutOffset = Offset * Weight;
    OutRoll = Roll * Weight;
}

FMinimalViewInfo FCandyFightFireCameraState::Compose(const FMinimalViewInfo& Base, const FVector2D& Offset, double Roll)
{
    FMinimalViewInfo Result = Base;
    const FQuat Rotation = Base.Rotation.Quaternion();
    Result.Location += Rotation.GetRightVector() * Offset.X + Rotation.GetUpVector() * Offset.Y;
    // Local X is the camera's view axis. Preserve its forward direction exactly.
    if (Roll != 0.) Result.Rotation = (Rotation * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Roll))).Rotator();
    return Result;
}

UCandyFightFireCameraComponent::UCandyFightFireCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEvenWhenPaused = true; // Reapply the frozen pose if the camera is evaluated while paused.
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCandyFightFireCameraComponent::BeginPlay()
{
    Super::BeginPlay();
    Random.GenerateNewSeed();
    if (auto* Character = Cast<ACandyFightCharacter>(GetOwner())) AddTickPrerequisiteComponent(Character->Combat);
}

void UCandyFightFireCameraComponent::NotifyShot(const UCandyFightWeaponConfig& Config, double Now, const FVector& ShotDirection)
{
    if (State.Trigger(Config, Random, Now, GFrameCounter,
        FCandyFightFireCameraState::ProjectRecoil(ShotDirection, LogicalRotation))) ++ShakeRequests;
}

void UCandyFightFireCameraComponent::RecordLogicalCamera(uint64 Frame, const FRotator& Rotation)
{
    LogicalRotation = Rotation.Quaternion();
    LogicalFrame = Frame;
    ++Revision;
}

void UCandyFightFireCameraComponent::ApplyVisualCamera(double Now, uint64 Frame)
{
    const auto* Character = Cast<ACandyFightCharacter>(GetOwner());
    auto* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
    auto* Manager = PC ? PC->PlayerCameraManager.Get() : nullptr;
    if (!PC || !PC->IsLocalController() || PC->GetPawn() != Character || !Manager || PC->GetViewTarget() != Character)
    {
        ClearFeedback();
        return;
    }
    // No fresh logical view means no safe base for an additive write (e.g. a view blend).
    if (LogicalFrame != Frame) return;
    if (AppliedRevision != Revision || WrittenManager.Get() != Manager)
    {
        BaseView = Manager->GetCameraCacheView();
        AppliedRevision = Revision;
    }
    double Roll;
    State.Evaluate(Now, CurrentOffset, Roll);
    CurrentRoll = Roll;
    WrittenView = FCandyFightFireCameraState::Compose(BaseView, CurrentOffset, Roll);
    Manager->SetCameraCachePOV(WrittenView);
    WrittenManager = Manager;
    bWroteCamera = true;
}

void UCandyFightFireCameraComponent::ClearFeedback()
{
    // Restore only our own last output, never a newer camera or another view target.
    if (auto* Manager = WrittenManager.Get(); bWroteCamera && Manager && Manager->GetViewTarget() == GetOwner())
    {
        const auto& Current = Manager->GetCameraCacheView();
        if (Current.Location.Equals(WrittenView.Location, .000001) && Current.Rotation.Equals(WrittenView.Rotation, .000001))
        {
            FMinimalViewInfo Restored = Current;
            Restored.Location = BaseView.Location;
            Restored.Rotation = BaseView.Rotation;
            Manager->SetCameraCachePOV(Restored);
        }
    }
    State.Reset();
    CurrentOffset = FVector2D::ZeroVector;
    CurrentRoll = 0.f;
    bWroteCamera = false;
    WrittenManager.Reset();
    AppliedRevision = MAX_uint64;
}

void UCandyFightFireCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    ApplyVisualCamera(GetWorld()->GetTimeSeconds(), GFrameCounter);
}

void UCandyFightFireCameraComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearFeedback();
    Super::EndPlay(Reason);
}

void UCandyFightFireCameraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ClearFeedback();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}
