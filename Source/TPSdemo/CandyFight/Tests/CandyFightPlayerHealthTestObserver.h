#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CandyFightPlayerHealthTestObserver.generated.h"

UCLASS()
class UCandyFightPlayerHealthTestObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 ChangeCount = 0;
    int32 DamageCount = 0;
    float TotalDamage = 0.f;
    UFUNCTION() void Changed(float Current, float Max) { ++ChangeCount; }
    UFUNCTION() void Damaged(float AppliedDamage) { ++DamageCount; TotalDamage += AppliedDamage; }
};
