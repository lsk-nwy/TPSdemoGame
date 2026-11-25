#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Weapons/CandyFightWeaponConfig.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "Components/LineBatchComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ACandyFightProjectileShot::ACandyFightProjectileShot()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    Tracers = CreateDefaultSubobject<ULineBatchComponent>(TEXT("Tracers"));
    SetRootComponent(Tracers);
    Tracers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tracers->SetGenerateOverlapEvents(false);
    Tracers->SetCanEverAffectNavigation(false);
    // This actor owns line lifetime; no real-time/independent component expiration on pause.
    Tracers->PrimaryComponentTick.bCanEverTick = false;
}

void ACandyFightProjectileShot::Initialize(const UCandyFightWeaponConfig& Config, const FVector& Start,
    const TArray<FVector>& Directions, AActor* Shooter, AController* Controller,
    UCandyFightCombatComponent* Combat, AActor* Gun, double Now)
{
    static uint64 NextShotId = 0; // Game-thread only, unique across shooters and PIE worlds.
    ShotId = ++NextShotId;
    Source = Shooter;
    SourceController = Controller;
    SourceCombat = Combat;
    SourceGun = Gun;
    Speed = Config.ProjectileSpeed;
    Range = Config.Range;
    Damage = Config.Damage;
    // 元素在开火瞬间快照：之后切元素不会改变已在飞行的子弹。
    Element = Combat ? Combat->GetElement() : ECandyFightElement::None;
    ElementGauge = FMath::IsFinite(Config.ElementGauge) && Config.ElementGauge > 0.f ? Config.ElementGauge : 1.f;
    TracerLength = Config.TracerLength;
    TracerThickness = Config.TracerThickness;
    TracerColor = Config.TracerColor;
    ImpactEffect = Config.ImpactEffect;
    ImpactEffectScale = Config.ImpactEffectScale;
    LastTime = Now;
    for (const FVector& Direction : Directions)
    {
        FCandyFightFlyingPellet Pellet;
        Pellet.Position = Start;
        Pellet.Direction = Direction.GetSafeNormal2D();
        Pellets.Add(Pellet);
    }
    bInitialized = true;
    if (Combat) AddTickPrerequisiteComponent(Combat);
}

void ACandyFightProjectileShot::DispatchDamage(const TMap<TWeakObjectPtr<AActor>, float>& Hits)
{
    for (const auto& Pair : Hits)
    {
        if (AActor* Target = Pair.Key.Get())
        {
            FCandyFightProjectileDamageEvent Event;
            Event.DamageTypeClass = UDamageType::StaticClass();
            Event.ShotId = ShotId;
            Event.Element = Element;
            Event.ElementGauge = ElementGauge;
            Event.CumulativeDamage = (CumulativeDamage.FindOrAdd(Target) += Pair.Value);
            const float Applied = Target->TakeDamage(Pair.Value, Event, SourceController.Get(), this);
            if (Applied > 0.f && SourceCombat.IsValid()) SourceCombat->OnHit.Broadcast(Target, Applied);
        }
    }
}

bool ACandyFightProjectileShot::BuildImpactTransform(const FHitResult& Hit, float Scale, FTransform& OutTransform)
{
    // Start-penetrating sweeps do not provide a reliable surface impact point.
    if (!Hit.bBlockingHit || Hit.bStartPenetrating || Hit.ImpactPoint.ContainsNaN() ||
        Hit.ImpactNormal.ContainsNaN() || !FMath::IsFinite(Scale) || Scale <= 0.f) return false;
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero()) return false;
    OutTransform = FTransform(Normal.Rotation(), Hit.ImpactPoint + Normal, FVector(Scale));
    return true;
}

void ACandyFightProjectileShot::PlayImpact(const FHitResult& Hit)
{
    if (!ImpactEffect || !BuildImpactTransform(Hit, ImpactEffectScale, LastImpactTransform)) return;
    ++ImpactEffectRequests;
    // Niagara owns this through WorldSettings, never through the short-lived shot.
    // No attachment/pooling: the system completes and auto-destroys in world space.
    UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactEffect,
        LastImpactTransform.GetLocation(), LastImpactTransform.Rotator(), LastImpactTransform.GetScale3D(),
        true, false, ENCPoolMethod::None, false);
    LastImpactComponent = Effect;
    if (Effect)
    {
        Effect->SetTickableWhenPaused(false);
        Effect->Activate(true);
    }
}

void ACandyFightProjectileShot::ResolveMuzzleObstruction(const FHitResult& Hit)
{
    if (!bInitialized || bFinished) return;
    TMap<TWeakObjectPtr<AActor>, float> Hits;
    AActor* Target = Hit.GetActor();
    if (IsValid(Target)) Hits.Add(Target, Damage * Pellets.Num());
    for (auto& Pellet : Pellets)
    {
        if (Pellet.bActive) PlayImpact(Hit);
        Pellet.bActive = false;
    }
    DispatchDamage(Hits);
    Finish();
}

void ACandyFightProjectileShot::AdvanceTo(double Now)
{
    if (!bInitialized || bFinished) return;
    if (!Source.IsValid()) { Finish(); return; }
    if (!FMath::IsFinite(Now) || Now <= LastTime) return;
    const double Travel = Speed * (Now - LastTime);
    LastTime = Now;
    Tracers->Flush();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightProjectile), false, Source.Get());
    Params.AddIgnoredActor(this);
    if (SourceGun.IsValid()) Params.AddIgnoredActor(SourceGun.Get());
    if (SourceCombat.IsValid() && SourceCombat->EquippedWeapon) Params.AddIgnoredActor(SourceCombat->EquippedWeapon.Get());
    TMap<TWeakObjectPtr<AActor>, float> Hits;
    bool bAnyActive = false;
    for (auto& Pellet : Pellets)
    {
        if (!Pellet.bActive) continue;
        const double Step = FMath::Min(Travel, FMath::Max(0., Range - Pellet.Distance));
        FHitResult Hit;
        FVector End;
        UCandyFightCombatComponent::TracePellet(GetWorld(), Pellet.Position, Pellet.Direction, Step, Params, Hit, End);
        Pellet.Distance += Hit.bBlockingHit ? (End - Pellet.Position).Size() : Step;
        Pellet.Position = End;
        if (Hit.bBlockingHit)
        {
            Pellet.bActive = false;
            PlayImpact(Hit);
            if (Hit.GetActor()) Hits.FindOrAdd(Hit.GetActor()) += Damage;
        }
        else if (Pellet.Distance >= Range - UE_SMALL_NUMBER) Pellet.bActive = false;
        if (Pellet.bActive)
        {
            bAnyActive = true;
            if (TracerLength > 0.f)
                Tracers->DrawLine(End - Pellet.Direction * FMath::Min(double(TracerLength), Pellet.Distance), End,
                    TracerColor, SDPG_World, TracerThickness, -1.f);
        }
    }
    // Trace the entire frame first so killing a target cannot let sibling pellets through it.
    DispatchDamage(Hits);
    if (!bAnyActive) Finish();
}

void ACandyFightProjectileShot::Finish()
{
    bFinished = true;
    Tracers->Flush();
    Destroy();
}

void ACandyFightProjectileShot::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AdvanceTo(GetWorld()->GetTimeSeconds());
}

void ACandyFightProjectileShot::EndPlay(const EEndPlayReason::Type Reason)
{
    bFinished = true;
    Tracers->Flush();
    Pellets.Reset();
    CumulativeDamage.Reset();
    Super::EndPlay(Reason);
}
