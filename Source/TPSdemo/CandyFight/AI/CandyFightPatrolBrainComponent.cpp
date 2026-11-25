#include "CandyFightPatrolBrainComponent.h"
#include "Engine/World.h"
void UCandyFightPatrolBrainComponent::SetWeaponTestPaused(bool bPaused)
{
    if (bPaused == bWeaponTestPaused) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (bPaused) { PauseStarted = Now; PauseLogic(TEXT("Weapon test")); }
    else
    {
        if (LastActiveTick >= 0.) LastActiveTick += Now - PauseStarted;
        ResumeLogic(TEXT("Weapon test finished"));
    }
    bWeaponTestPaused = bPaused;
}
double UCandyFightPatrolBrainComponent::ActiveDelta(double Now, float FallbackDelta)
{
    if (bWeaponTestPaused) return 0.;
    const double Delta = LastActiveTick >= 0. ? FMath::Max(0., Now - LastActiveTick) : FallbackDelta;
    LastActiveTick = Now;
    return Delta;
}
void UCandyFightPatrolBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    if (bWeaponTestPaused) return;
    Super::TickComponent(float(ActiveDelta(GetWorld()->GetTimeSeconds(), DeltaTime)), TickType, TickFunction);
}
