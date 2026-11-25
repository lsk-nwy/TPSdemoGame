#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"
#include "CandyFightCombatOwner.h"
#include "CandyFightWeapon.h"
#include "Sound/SoundAttenuation.h"

void UCandyFightCombatComponent::StopAudio(TWeakObjectPtr<UAudioComponent>& Component)
{
    // Immediate cancellation (0ms) also works while paused or the device is suspended.
    if (auto* Audio = Component.Get()) { Audio->Stop(); Audio->DestroyComponent(); }
    Component.Reset();
}

void UCandyFightCombatComponent::StopWeaponAudio()
{
    StopAudio(ActionAudio);
    for (auto& Audio : FadingActionAudio) StopAudio(Audio);
    FadingActionAudio.Reset();
    StopAudio(DryAudio);
    for (auto& Audio : ShotAudio) StopAudio(Audio);
    ShotAudio.Reset();
    AudioSchedule.Reset();
}

void UCandyFightCombatComponent::CancelActionAudio()
{
    if (auto* Audio = ActionAudio.Get(); Audio && Audio->IsPlaying() &&
        FApp::HasFocus() && !UGameplayStatics::IsGamePaused(this))
    {
        // Keep ownership until the short fade completes so equip/end-play can still stop it.
        Audio->FadeOut(.015f, 0.f);
        FadingActionAudio.Add(Audio);
        ActionAudio.Reset();
    }
    else StopAudio(ActionAudio);
}

UAudioComponent* UCandyFightCombatComponent::CreateWeaponAudio(USoundBase* Sound, float Volume, float Offset, bool bFollowOwner)
{
    const auto* Pawn = Cast<APawn>(GetOwner());
    if (!Sound || Volume <= 0.f || !Pawn || !FApp::HasFocus()) return nullptr;
    if (IsExternallyDriven())
    {
        const auto* Owner = Cast<ICandyFightCombatOwner>(GetOwner());
        if (!Owner || !EquippedWeapon || !EquippedWeapon->ValidateMuzzle()) return nullptr;
        auto* Audio = NewObject<UAudioComponent>(GetOwner());
        Audio->bAutoActivate = false;
        Audio->bAutoDestroy = true;
        Audio->bIsUISound = false;
        Audio->bAllowSpatialization = true;
        Audio->AttenuationSettings = Owner->GetCombatAttenuation();
        Audio->SetSound(Sound);
        Audio->SetVolumeMultiplier(Volume);
        if (bFollowOwner) Audio->SetupAttachment(EquippedWeapon->Muzzle);
        Audio->RegisterComponentWithWorld(GetWorld());
        if (!bFollowOwner) Audio->SetWorldLocation(EquippedWeapon->Muzzle->GetComponentLocation());
        Audio->Play(Offset);
        return Audio;
    }
    if (!Pawn->IsLocallyControlled()) return nullptr;
    auto* Audio = UGameplayStatics::CreateSound2D(this, Sound, Volume, 1.f, 0.f, nullptr, false, true);
    if (Audio)
    {
        Audio->bIsUISound = false;
        Audio->bAllowSpatialization = false;
        Audio->Play(Offset);
    }
    return Audio;
}

void UCandyFightCombatComponent::PlayFireAudio()
{
    ++FireSoundRequests; // One request per committed shot, independent of device availability.
    ShotAudio.RemoveAll([](const auto& Audio) { return !Audio.IsValid() || !Audio->IsPlaying(); });
    if (ShotAudio.Num() >= 16) { StopAudio(ShotAudio[0]); ShotAudio.RemoveAt(0); }
    if (const auto* Config = GetCurrentConfig())
        if (auto* Audio = CreateWeaponAudio(Config->FireSound, Config->FireVolume)) ShotAudio.Add(Audio);
}

void UCandyFightCombatComponent::SyncStateEvents()
{
    if (!WeaponStates.IsValidIndex(EquippedIndex)) return;
    const auto& State = WeaponStates[EquippedIndex];
    if (ObservedPhaseSerial != State.PhaseSerial)
    {
        CancelActionAudio();
        ObservedPhaseSerial = State.PhaseSerial;
        OnReloadPhaseChanged.Broadcast();
        OnReloadChanged.Broadcast();
    }
    if (bObservedDry != State.bDryFiring)
    {
        bObservedDry = State.bDryFiring;
        if (!State.bDryFiring) StopAudio(DryAudio);
        OnDryFireChanged.Broadcast();
        OnReloadChanged.Broadcast(); // Existing HUD subscribers remain compatible.
    }
}

void UCandyFightCombatComponent::UpdateWeaponAudio(double Now)
{
    FadingActionAudio.RemoveAll([](const auto& Audio) { return !Audio.IsValid() || !Audio->IsPlaying(); });
    const auto* Config = GetCurrentConfig();
    if (!Config || !WeaponStates.IsValidIndex(EquippedIndex)) return;
    const auto& State = WeaponStates[EquippedIndex];
    if (!FApp::HasFocus())
    {
        StopWeaponAudio(); // Returning resumes only the current action at its elapsed offset.
        return;
    }
    USoundBase* Action = nullptr;
    float Volume = Config->ReloadVolume;
    switch (State.ReloadPhase)
    {
    case ECandyFightReloadPhase::Magazine: Action = Config->ReloadSound; break;
    case ECandyFightReloadPhase::Shell: Action = Config->ShellSound; Volume = Config->ShellVolume; break;
    default: break;
    }
    double Offset = 0.;
    if (AudioSchedule.ConsumeAction(State, Now, Action ? Action->GetDuration() : 0., Offset))
    {
        StopAudio(ActionAudio);
        ++ActionSoundRequests;
        ActionAudio = CreateWeaponAudio(Action, Volume, float(Offset), true);
    }
    if (!State.bReloading) StopAudio(ActionAudio);
    const double DryDuration = Config->DryFireSound ? Config->DryFireSound->GetDuration() : .857324;
    if (AudioSchedule.ConsumeDry(State, Now, DryDuration))
    {
        StopAudio(DryAudio);
        ++DrySoundRequests;
        DryAudio = CreateWeaponAudio(Config->DryFireSound, Config->DryFireVolume);
    }
    if (!State.bDryFiring) StopAudio(DryAudio);
}
