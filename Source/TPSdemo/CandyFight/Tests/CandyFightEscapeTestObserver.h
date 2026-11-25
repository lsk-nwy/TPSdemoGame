#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CandyFightEscapeTestObserver.generated.h"
class ACandyFightCharacter;
class ACandyFightGameMode;

UCLASS()
class UCandyFightEscapeTestObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 ClosedCount=0;
    int32 EscapedCount=0;
    int32 CountdownCount=0;
    bool bStateCommitted=false;
    bool bCountdownCommitted=false;
    UPROPERTY() TObjectPtr<ACandyFightGameMode> Mode;
    UFUNCTION() void Closed() { ++ClosedCount; }
    UFUNCTION() void Escaped(ACandyFightCharacter* Player);
    UFUNCTION() void Countdown();
};
