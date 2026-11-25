#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CandyFightEnemyWeaponComponent.generated.h"

// 只决定何时扣扳机；弹药、射速、散布和子弹仍由 Combat 处理。
USTRUCT()
struct TPSDEMO_API FCandyFightBurstState
{
    GENERATED_BODY()
    int32 ShotsInGroup = 0;
    int32 CompletedGroups = 0;
    double NextGroupTime = 0.;
    double LastShotTime = -1.;
    void CommitShot(int32 Limit, double Pause, bool bEmpty, double Now)
    {
        LastShotTime = Now;
        if (++ShotsInGroup >= Limit || bEmpty)
        {
            ShotsInGroup = 0; ++CompletedGroups; NextGroupTime = Now + Pause;
        }
    }
    void Interrupt(double Pause)
    {
        if (ShotsInGroup > 0 && LastShotTime >= 0.) NextGroupTime = FMath::Max(NextGroupTime, LastShotTime + Pause);
        Stop();
    }
    void Stop() { ShotsInGroup = 0; } // 保留尚未结束的组间冷却。
};

UCLASS(ClassGroup=(CandyFight), meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightEnemyWeaponComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCandyFightEnemyWeaponComponent();
    bool StartTest(FString& Error);
    void InterruptAutonomousBurst();
    void StopTest(bool bResumePatrol = true);
    bool IsTesting() const { return bTesting; }
    FCandyFightBurstState Burst;
    FVector AimLocation = FVector::ZeroVector;
    uint64 AimFrame = MAX_uint64;
    bool bValidAim = false;
    // 独立入口便于不启动 PIE 的确定性验证。
    void UpdateDriver(double Now);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    friend class FCandyFightAutonomyTest;
    friend class FCandyFightEnemyWeaponIntegrationTest;
    bool bTesting = false;
    UFUNCTION() void HandleShot(int32 WeaponNumber, int32 Pellets);
};
