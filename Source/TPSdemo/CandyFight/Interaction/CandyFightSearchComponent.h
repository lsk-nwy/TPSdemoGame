#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CandyFightSearchComponent.generated.h"

class ACandyFightChest;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
struct FInputActionValue;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCandyFightSearchChanged);

UCLASS(ClassGroup=(CandyFight), meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightSearchComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCandyFightSearchComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> InteractAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> InteractMapping;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="0",Units="s")) float TargetRefreshSeconds = .1f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Interaction") TObjectPtr<ACandyFightChest> Target;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Interaction") bool bSearching = false;
    UPROPERTY(BlueprintAssignable, Category="Interaction") FCandyFightSearchChanged OnSearchChanged;
    UFUNCTION(BlueprintCallable, Category="Interaction") bool TryBeginSearch();
    UFUNCTION(BlueprintCallable, Category="Interaction") void CancelSearch();
    UFUNCTION(BlueprintPure, Category="Interaction") float GetSearchProgress() const;
    UFUNCTION(BlueprintPure, Category="Interaction") FText GetInteractKeyText() const;
    void BindInput(UEnhancedInputComponent* Input);
    void InitializeLocalPlayer();
    void RefreshTarget();
    // World-time driven entry point also used by deterministic automation.
    void UpdateSearch();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RegisteredMapping;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    double SearchStart = 0.;
    float SearchDuration = 5.f;
    double NextTargetRefresh = 0.;
    bool bCheckPhysicalInput = false;
    FDelegateHandle ActivationHandle;
    void RemoveMapping();
    void FinishSearchState();
    void HandlePress(const FInputActionValue& Value);
    void HandleRelease(const FInputActionValue& Value);
    void OnActivationChanged(bool bActive);
    UFUNCTION() void HandleDamage(float AppliedDamage);
};
