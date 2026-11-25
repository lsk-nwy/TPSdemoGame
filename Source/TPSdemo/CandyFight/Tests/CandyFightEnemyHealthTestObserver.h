#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CandyFightEnemyHealthTestObserver.generated.h"

class ACandyFightEnemyCharacter;
class AController;

// Reflected receiver for testing the Blueprint multicast contract, including reentrant damage.
UCLASS()
class UCandyFightEnemyHealthTestObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 DefeatCount = 0;
    float ReentrantDamage = -1.f;
    UFUNCTION() void Defeated(ACandyFightEnemyCharacter* Enemy, AController* InstigatorController);
};
