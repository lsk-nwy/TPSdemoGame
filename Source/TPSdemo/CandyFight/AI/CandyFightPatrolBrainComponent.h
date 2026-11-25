#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "CandyFightPatrolBrainComponent.generated.h"

// BT 可以按需低频 Tick。恢复时剔除暂停时长，避免 Wait 吃进一大段停火测试时间。
UCLASS()
class TPSDEMO_API UCandyFightPatrolBrainComponent : public UBehaviorTreeComponent
{
    GENERATED_BODY()
public:
    void SetWeaponTestPaused(bool bPaused);
    double ActiveDelta(double Now, float FallbackDelta);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    bool bWeaponTestPaused = false;
    double PauseStarted = 0.;
    double LastActiveTick = -1.;
};
