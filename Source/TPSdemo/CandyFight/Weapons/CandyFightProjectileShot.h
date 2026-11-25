#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DamageEvents.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFightProjectileShot.generated.h"

class UCandyFightWeaponConfig;
class UCandyFightCombatComponent;
class ULineBatchComponent;
class UNiagaraSystem;
class UNiagaraComponent;

/** Standard damage event with per-shot metadata; ordinary TakeDamage receivers remain supported. */
struct FCandyFightProjectileDamageEvent : public FDamageEvent
{
    static constexpr int32 ClassID = 7301;
    uint64 ShotId = 0;
    float CumulativeDamage = 0.f;
    /** 本次命中携带的元素；None 表示物理。 */
    ECandyFightElement Element = ECandyFightElement::None;
    /** 本次命中写入附着层的元素量（GU）。 */
    float ElementGauge = 1.f;
    virtual int32 GetTypeID() const override { return ClassID; }
    virtual bool IsOfType(int32 ID) const override { return ID == ClassID || FDamageEvent::IsOfType(ID); }
};

struct FCandyFightFlyingPellet
{
    FVector Position = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
    double Distance = 0.;
    bool bActive = true;
};

/** One shot owns all its pellets, immutable launch settings and transient tracer geometry. */
UCLASS(NotBlueprintable, Transient)
class TPSDEMO_API ACandyFightProjectileShot : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightProjectileShot();
    void Initialize(const UCandyFightWeaponConfig& Config, const FVector& Start, const TArray<FVector>& Directions,
        AActor* Shooter, AController* Controller, UCandyFightCombatComponent* Combat, AActor* Gun, double Now);
    // Also used by deterministic headless tests. Now is absolute game time, never wall clock.
    void AdvanceTo(double Now);
    void ResolveMuzzleObstruction(const FHitResult& Hit);
    static bool BuildImpactTransform(const FHitResult& Hit, float Scale, FTransform& OutTransform);
    /** Number of visual requests, independent of rendering/culling and merged damage. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") int32 ImpactEffectRequests = 0;
    const TArray<FCandyFightFlyingPellet>& GetPellets() const { return Pellets; }
    uint64 GetShotId() const { return ShotId; }
    bool IsFinished() const { return bFinished; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<ULineBatchComponent> Tracers;
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FCandyFightProjectileImpactTest;
    UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> ImpactEffect;
    float ImpactEffectScale = 1.f;
    FTransform LastImpactTransform;
    TWeakObjectPtr<UNiagaraComponent> LastImpactComponent;
    void PlayImpact(const FHitResult& Hit);
    TArray<FCandyFightFlyingPellet> Pellets;
    TMap<TWeakObjectPtr<AActor>, float> CumulativeDamage;
    TWeakObjectPtr<AActor> Source;
    TWeakObjectPtr<AController> SourceController;
    TWeakObjectPtr<UCandyFightCombatComponent> SourceCombat;
    TWeakObjectPtr<AActor> SourceGun;
    double LastTime = 0.;
    double Speed = 5000.;
    double Range = 5000.;
    float Damage = 0.f;
    ECandyFightElement Element = ECandyFightElement::None;
    float ElementGauge = 1.f;
    float TracerLength = 30.f;
    float TracerThickness = 1.2f;
    FLinearColor TracerColor;
    uint64 ShotId = 0;
    bool bInitialized = false;
    bool bFinished = false;
    void DispatchDamage(const TMap<TWeakObjectPtr<AActor>, float>& Hits);
    void Finish();
};
