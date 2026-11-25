#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CandyFightEscapeGate.generated.h"

class ACandyFightCharacter;
class UStaticMeshComponent;
class USphereComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
UENUM(BlueprintType)
enum class ECandyFightEscapeGateState : uint8 { Open, Closed };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCandyFightGateClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightPlayerEscaped,ACandyFightCharacter*,Player);

UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightEscapeGate : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightEscapeGate();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape Gate") TObjectPtr<UStaticMeshComponent> Disc;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape Gate") TObjectPtr<USphereComponent> RangePreview;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape Gate") TObjectPtr<USceneComponent> InteractionPoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape Gate") TObjectPtr<UWidgetComponent> GateWidget;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Escape Gate",meta=(ExposeOnSpawn="true",ClampMin="0.01",Units="s")) float OpenSeconds=120.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Escape Gate",meta=(ExposeOnSpawn="true",ClampMin="0.01",Units="s")) float EscapeSeconds=5.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Escape Gate",meta=(ExposeOnSpawn="true",ClampMin="1",Units="cm")) float EscapeRadius=150.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Escape Gate",meta=(ExposeOnSpawn="true",ClampMin="0",Units="cm")) float MaxHeightDifference=120.f;
    UPROPERTY(Transient,VisibleInstanceOnly,BlueprintReadOnly,Category="Escape Gate") ECandyFightEscapeGateState State=ECandyFightEscapeGateState::Closed;
    UPROPERTY(BlueprintAssignable,Category="Escape Gate") FCandyFightGateClosed OnClosed;
    UPROPERTY(BlueprintAssignable,Category="Escape Gate") FCandyFightPlayerEscaped OnPlayerEscaped;
    UFUNCTION(BlueprintPure,Category="Escape Gate") bool IsOpen() const;
    UFUNCTION(BlueprintPure,Category="Escape Gate") float GetRemainingCloseSeconds() const;
    UFUNCTION(BlueprintCallable,Category="Escape Gate") void CloseGate();
    bool ValidateConfiguration(FString& Error) const;
    bool CanEscape(const ACandyFightCharacter* Player) const;
    float GetEscapeDuration() const { return RuntimeEscapeSeconds; }
    bool CompleteEscape(ACandyFightCharacter* Player);
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool bStarted=false;
    bool bClosedNotified=false;
    double CloseTime=0.;
    double SuccessUntil=0.;
    float RuntimeEscapeSeconds=5.f;
    float RuntimeRadius=150.f;
    float RuntimeHeight=120.f;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DiscMaterial;
    void UpdatePresentation();
};
