#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UCandyFightWeaponConfig::ValidateConfiguration(FString& Error) const
{
    Error.Reset();
    const auto Nonnegative = [](float V) { return FMath::IsFinite(V) && V >= 0.f; };
    if (!WeaponClass || WeaponClass->HasAnyClassFlags(CLASS_Abstract))
        Error = TEXT("Assign a concrete CandyFight weapon blueprint class.");
    else if (!FMath::IsFinite(ShotsPerSecond) || ShotsPerSecond <= 0.f || ShotsPerSecond > 60.f ||
        MagazineCapacity < 1 || !FMath::IsFinite(ReloadSeconds) || ReloadSeconds <= 0.f ||
        !Nonnegative(Damage) || !FMath::IsFinite(Range) || Range <= 0.f)
        Error = TEXT("Weapon rate (0,60], magazine, reload time and range must be positive and finite; damage must be nonnegative.");
    else if (!Nonnegative(AimAssistPaddingPixels) || !Nonnegative(AimAssistSwitchBiasPixels) ||
        !Nonnegative(AimAssistSnapSeconds) || !Nonnegative(AimAssistReleasePaddingPixels))
        Error = TEXT("Aim assist padding, switch bias, snap time and release padding must be finite and nonnegative.");
    else if (!FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed <= 0.f || !Nonnegative(TracerLength) ||
        !FMath::IsFinite(TracerThickness) || TracerThickness <= 0.f ||
        !Nonnegative(TracerColor.R) || !Nonnegative(TracerColor.G) || !Nonnegative(TracerColor.B) || !Nonnegative(TracerColor.A))
        Error = TEXT("Projectile speed and tracer thickness must be positive and finite; tracer length and color must be finite and nonnegative.");
    else if (!FMath::IsFinite(ImpactEffectScale) || ImpactEffectScale <= 0.f)
        Error = TEXT("Impact effect scale must be finite and positive.");
    else if (FireShakeMaxOffset.ContainsNaN() || FireShakeMaxOffset.X < 0. || FireShakeMaxOffset.Y < 0. ||
        !Nonnegative(FireShakeMaxRollDegrees) || !FMath::IsFinite(FireShakeReturnSeconds) || FireShakeReturnSeconds <= 0.f ||
        !Nonnegative(FireShakeDirectionJitterDegrees) || FireShakeDirectionJitterDegrees > 45.f)
        Error = TEXT("Fire camera offset and roll must be finite and nonnegative; return time must be finite and positive; direction jitter must be finite in [0,45] degrees.");
    else if ((FireMode == ECandyFightFireMode::Shotgun && !Nonnegative(ShellReloadStartDelay)) || !Nonnegative(FireVolume) ||
        !Nonnegative(ReloadVolume) || !Nonnegative(ShellVolume) || !Nonnegative(DryFireVolume))
        Error = TEXT("Shotgun preparation delay and active audio volumes must be nonnegative and finite.");
    else if (PelletCount < 1 || PelletCount > 64 ||
        (FireMode != ECandyFightFireMode::Shotgun && PelletCount != 1))
        Error = TEXT("Shotguns require 1-64 pellets; other fire modes require exactly one pellet.");
    else if (!FMath::IsFinite(ElementGauge) || ElementGauge < 0.1f || ElementGauge > 4.f)
        Error = TEXT("Element gauge must be finite and within [0.1, 4] GU.");
    else if (FireMode == ECandyFightFireMode::Shotgun &&
        (!FMath::IsFinite(ShotgunCenterWidthRatio) || ShotgunCenterWidthRatio <= 0.f || ShotgunCenterWidthRatio >= 1.f ||
        ShotgunSidePelletCount < 0 || ShotgunSidePelletCount > (PelletCount - 1) / 2))
        Error = TEXT("Shotgun center width ratio must be finite and strictly between 0 and 1; side counts must be nonnegative and leave at least one center pellet.");
    else if (!Nonnegative(BaseSpread) || !Nonnegative(MaxSpread) || MaxSpread < BaseSpread || MaxSpread > 45.f ||
        !Nonnegative(SpreadPerShot) || !Nonnegative(RecoveryDelay) || !Nonnegative(RecoveryPerSecond))
        Error = TEXT("Spread must be finite, 0 <= base <= max <= 45 degrees; recovery values must be nonnegative.");
    else if (FireMode != ECandyFightFireMode::SemiAutomatic && FireMode != ECandyFightFireMode::Automatic && FireMode != ECandyFightFireMode::Shotgun)
        Error = TEXT("Unknown fire mode.");
    return Error.IsEmpty();
}

#if WITH_EDITOR
EDataValidationResult UCandyFightWeaponConfig::IsDataValid(FDataValidationContext& Context) const
{
    FString Error;
    if (!ValidateConfiguration(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
    return EDataValidationResult::Valid;
}
#endif

void FCandyFightWeaponState::Initialize(const UCandyFightWeaponConfig& Config, double Now)
{
    *this = FCandyFightWeaponState();
    Ammo = Config.MagazineCapacity;
    Spread = Config.BaseSpread;
    LastUpdateTime = Now;
}

bool FCandyFightWeaponState::Advance(const UCandyFightWeaponConfig& Config, double Now)
{
    const double RecoverFrom = FMath::Max(LastUpdateTime, LastShotTime + Config.RecoveryDelay);
    Spread = FMath::Clamp(Spread - float(FMath::Max(0., Now - RecoverFrom)) * Config.RecoveryPerSecond,
        Config.BaseSpread, Config.MaxSpread);
    LastUpdateTime = Now;
    bNeedsRack = false; // Legacy state must never impose a shooting lock.
    if (ReloadPhase == ECandyFightReloadPhase::Rack)
    {
        SetPhase(ECandyFightReloadPhase::Ready, Now, 0.);
        return true;
    }
    if (!bReloading || Now + .000001 < ReloadEndTime) return false;
    // Preserve deadlines across normal frame quantization; rebase after a full-cycle hitch.
    const double NextStart = Now - ReloadEndTime < Config.ReloadSeconds ? ReloadEndTime : Now;
    if (ReloadPhase == ECandyFightReloadPhase::ShellStart)
    {
        SetPhase(ECandyFightReloadPhase::Shell, NextStart, Config.ReloadSeconds);
    }
    else if (ReloadPhase == ECandyFightReloadPhase::Shell)
    {
        Ammo = FMath::Min(Ammo + 1, Config.MagazineCapacity);
        if (Ammo == Config.MagazineCapacity)
        {
            SetPhase(ECandyFightReloadPhase::Ready, Now, 0.);
        }
        else SetPhase(ECandyFightReloadPhase::Shell, NextStart, Config.ReloadSeconds);
    }
    else
    {
        Ammo = Config.MagazineCapacity;
        SetPhase(ECandyFightReloadPhase::Ready, Now, 0.);
    }
    return true;
}

bool FCandyFightWeaponState::CanFire(const UCandyFightWeaponConfig& Config, double Now) const
{
    // Tiny tolerance handles float world-time quantization without permitting frame-scale overspeed.
    return Ammo > 0 && (!bReloading || ReloadPhase == ECandyFightReloadPhase::Shell || ReloadPhase == ECandyFightReloadPhase::ShellStart)
        && Now + 0.0001 >= NextShotTime;
}

void FCandyFightWeaponState::CommitShot(const UCandyFightWeaponConfig& Config, double Now, bool bTriggerHeld)
{
    // Interrupt only when a shot actually fires, never for an empty/cooldown/invalid-aim click.
    CancelReload();
    const double Interval = 1. / Config.ShotsPerSecond;
    // Preserve the cadence through normal frame quantization instead of adding
    // a fraction of a frame to every automatic shot. Rebase after a hitch/idle;
    // never build a backlog or emit multiple shots in one frame.
    const bool bContinueCadence = Config.FireMode == ECandyFightFireMode::Automatic &&
        LastShotTime > -10000. && Now - NextShotTime <= Interval * .5;
    NextShotTime = bContinueCadence ? NextShotTime + Interval : Now + Interval;
    --Ammo;
    ++ShotsFired;
    LastShotTime = LastUpdateTime = Now;
    Spread = FMath::Min(Config.MaxSpread, Spread + Config.SpreadPerShot);
    if (Ammo == 0)
    {
        if (Config.FireMode == ECandyFightFireMode::Automatic && bTriggerHeld) bDryFiring = true;
        else BeginReload(Config, Now);
    }
}

bool FCandyFightWeaponState::BeginReload(const UCandyFightWeaponConfig& Config, double Now)
{
    if (bReloading || Ammo >= Config.MagazineCapacity) return false;
    bDryFiring = false;
    bNeedsRack = false;
    const bool bShotgun = Config.FireMode == ECandyFightFireMode::Shotgun;
    const bool bPrepare = bShotgun && Config.ShellReloadStartDelay > 0.f;
    SetPhase(bPrepare ? ECandyFightReloadPhase::ShellStart : bShotgun ? ECandyFightReloadPhase::Shell : ECandyFightReloadPhase::Magazine,
        Now, bPrepare ? Config.ShellReloadStartDelay : Config.ReloadSeconds);
    return true;
}

void FCandyFightWeaponState::SetPhase(ECandyFightReloadPhase Phase, double Now, double Duration)
{
    ReloadPhase = Phase;
    bReloading = Phase != ECandyFightReloadPhase::Ready;
    ReloadStartTime = Now;
    ReloadEndTime = Now + Duration;
    ++PhaseSerial;
}

void FCandyFightWeaponState::CancelReload()
{
    bNeedsRack = false;
    if (bReloading) SetPhase(ECandyFightReloadPhase::Ready, LastUpdateTime, 0.);
    bDryFiring = false;
}

void FCandyFightWeaponState::ResumeEquipped(const UCandyFightWeaponConfig& Config, double Now)
{
    Advance(Config, Now);
    if (Ammo == 0) BeginReload(Config, Now);
}

void FCandyFightWeaponState::ReleaseTrigger(const UCandyFightWeaponConfig& Config, double Now)
{
    if (bDryFiring) BeginReload(Config, Now);
}

bool FCandyFightAudioSchedule::ConsumeAction(const FCandyFightWeaponState& State, double Now, double Duration, double& Offset)
{
    if (!State.bReloading || (State.ReloadPhase != ECandyFightReloadPhase::Shell && State.ReloadPhase != ECandyFightReloadPhase::Magazine)
        || ActionSerial == State.PhaseSerial || Duration <= 0.) return false;
    const double Start = FMath::Max(State.ReloadStartTime, State.ReloadEndTime - Duration);
    if (Now < Start) return false;
    ActionSerial = State.PhaseSerial;
    Offset = Now - Start;
    return Now < State.ReloadEndTime && Offset < Duration;
}

bool FCandyFightAudioSchedule::ConsumeDry(const FCandyFightWeaponState& State, double Now, double Duration)
{
    if (!State.bDryFiring) { bWasDry = false; return false; }
    if (!bWasDry) { NextDryTime = State.NextShotTime; bWasDry = true; }
    if (Now + .0001 < NextDryTime) return false;
    NextDryTime = Now + FMath::Max(.01, Duration); // Rebase after hitches; never catch up.
    return true;
}

void CandyFightWeapons::SampleAngles(const UCandyFightWeaponConfig& Config, float Spread, FRandomStream& Random, TArray<float>& OutAngles)
{
    OutAngles.Reset();
    if (Config.FireMode != ECandyFightFireMode::Shotgun)
    {
        OutAngles.Add(Random.FRandRange(-Spread, Spread));
        return;
    }
    const float CenterHalfAngle = Spread * Config.ShotgunCenterWidthRatio;
    const auto SampleSector = [&](int32 Count, float MinAngle, float MaxAngle)
    {
        for (int32 Index = 0; Index < Count; ++Index)
            OutAngles.Add(Random.FRandRange(MinAngle, MaxAngle));
    };
    // Independent directions: no minimum spacing, mirror pairing or forced center pellet.
    SampleSector(Config.ShotgunSidePelletCount, -Spread, -CenterHalfAngle);
    SampleSector(Config.PelletCount - 2 * Config.ShotgunSidePelletCount, -CenterHalfAngle, CenterHalfAngle);
    SampleSector(Config.ShotgunSidePelletCount, CenterHalfAngle, Spread);
}
