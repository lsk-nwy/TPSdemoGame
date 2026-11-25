#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CandyFightPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCandyFightGoldChanged, int32, NewGold);
UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Gold", meta=(ClampMin="0")) int32 InitialGold = 0;
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="CandyFight|Gold") int32 Gold = 0;
    UPROPERTY(BlueprintAssignable, Category="CandyFight|Gold") FCandyFightGoldChanged OnGoldChanged;
    UFUNCTION(BlueprintCallable, Category="CandyFight|Gold") int32 AddGold(int32 Amount);
    UPROPERTY(Transient,VisibleInstanceOnly,BlueprintReadOnly,Category="CandyFight|Escape") bool bHasEscaped=false;
    // Gate commits this before broadcasting; ordinary display code cannot mark a player escaped.
    bool TryMarkEscaped();
protected:
    virtual void BeginPlay() override;
};
