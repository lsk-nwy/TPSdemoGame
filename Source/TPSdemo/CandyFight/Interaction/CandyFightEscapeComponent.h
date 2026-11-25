#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CandyFightEscapeComponent.generated.h"
class ACandyFightEscapeGate;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCandyFightEscapeChanged);

UCLASS(ClassGroup=(CandyFight),meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightEscapeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCandyFightEscapeComponent();
    UPROPERTY(Transient,BlueprintReadOnly,Category="Escape") TObjectPtr<ACandyFightEscapeGate> TargetGate;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Escape") bool bEscaping=false;
    UPROPERTY(BlueprintAssignable,Category="Escape") FCandyFightEscapeChanged OnEscapeChanged;
    UFUNCTION(BlueprintPure,Category="Escape") float GetEscapeProgress() const;
    UFUNCTION(BlueprintPure,Category="Escape") float GetRemainingEscapeSeconds() const;
    UFUNCTION(BlueprintCallable,Category="Escape") void CancelEscape();
    void UpdateEscape();
protected:
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    double EscapeStart=0.;
    float Duration=5.f;
    void ResetState();
};
