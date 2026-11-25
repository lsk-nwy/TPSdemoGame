#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFightTarget.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UCandyFightElementAuraComponent;
class UCandyFightElementReactionComponent;
class UCandyFightFrozenEffectComponent;

UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightTarget : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightTarget();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target") TObjectPtr<UBoxComponent> HitBox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target") TObjectPtr<UStaticMeshComponent> TargetMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target") TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target|Elements") TObjectPtr<UCandyFightElementAuraComponent> ElementAura;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target|Elements") TObjectPtr<UCandyFightElementReactionComponent> ElementReactions;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target|Elements") TObjectPtr<UCandyFightFrozenEffectComponent> ElementFrozen;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target", meta=(ClampMin="1")) float MaxHealth = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target", meta=(ClampMin="0.1")) float RespawnSeconds = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target") FLinearColor TargetColor = FLinearColor(.15f, .55f, .65f);
    UPROPERTY(Transient, BlueprintReadOnly, Category="Target") float Health = 200.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Target") float LastDamage = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Target") int32 HitCount = 0;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* InstigatorController, AActor* DamageCauser) override;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
    double FlashUntil = 0.;
    double DamageTextUntil = 0.;
    double RespawnAt = 0.;
    void UpdateLabel(bool bShowDamage);
    bool bShowingDamage = false;
    uint64 DisplayedShotId = 0;
};
