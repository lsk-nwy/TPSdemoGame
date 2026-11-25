#include "CandyFightEscapeComponent.h"
#include "CandyFightEscapeGate.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

UCandyFightEscapeComponent::UCandyFightEscapeComponent() { PrimaryComponentTick.bCanEverTick=true; }
float UCandyFightEscapeComponent::GetRemainingEscapeSeconds() const
{
    return bEscaping && GetWorld() ? float(FMath::Max(0.,double(Duration)-(GetWorld()->GetTimeSeconds()-EscapeStart))) : 0.f;
}
float UCandyFightEscapeComponent::GetEscapeProgress() const
{
    return bEscaping && Duration>0 ? FMath::Clamp(1.f-GetRemainingEscapeSeconds()/Duration,0.f,1.f) : 0.f;
}
void UCandyFightEscapeComponent::ResetState()
{
    if (!bEscaping && !TargetGate) return;
    bEscaping=false; TargetGate=nullptr; EscapeStart=0.; OnEscapeChanged.Broadcast();
}
void UCandyFightEscapeComponent::CancelEscape() { ResetState(); }
void UCandyFightEscapeComponent::UpdateEscape()
{
    auto* Player=Cast<ACandyFightCharacter>(GetOwner());
    auto* State=Player ? Player->GetPlayerState<ACandyFightPlayerState>() : nullptr;
    if (!IsValid(Player) || !Player->GetController() || !State || State->bHasEscaped) { ResetState(); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (TargetGate && (!IsValid(TargetGate) || !TargetGate->CanEscape(Player))) ResetState();
    if (!bEscaping)
    {
        ACandyFightEscapeGate* Nearest=nullptr;
        double Best=TNumericLimits<double>::Max();
        for (TActorIterator<ACandyFightEscapeGate> It(GetWorld());It;++It)
        {
            if (!It->CanEscape(Player)) continue;
            const double Distance=FVector::DistSquared2D(Player->GetActorLocation(),It->GetActorLocation());
            if (Distance<Best || (Distance==Best && Nearest && It->GetName()<Nearest->GetName())) { Best=Distance; Nearest=*It; }
        }
        if (!Nearest) return;
        TargetGate=Nearest; Duration=Nearest->GetEscapeDuration(); EscapeStart=GetWorld()->GetTimeSeconds();
        bEscaping=true; OnEscapeChanged.Broadcast();
    }
    if (bEscaping && IsValid(TargetGate) && GetRemainingEscapeSeconds()<=0.f)
    {
        // Gate rechecks its deadline and commits PlayerState before any callbacks.
        TargetGate->CompleteEscape(Player); ResetState();
    }
}
void UCandyFightEscapeComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction); UpdateEscape();
}
void UCandyFightEscapeComponent::EndPlay(const EEndPlayReason::Type Reason) { ResetState(); Super::EndPlay(Reason); }
