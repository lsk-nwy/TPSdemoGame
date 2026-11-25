#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/CameraTypes.h"
#include "CandyFightFireCameraComponent.generated.h"

class UCandyFightWeaponConfig;
class APlayerCameraManager;

/** Pure game-time response; its random stream is never shared with ballistics. */
struct FCandyFightFireCameraState
{
    FVector2D Offset = FVector2D::ZeroVector;
    double Roll = 0.;
    double StartedAt = 0.;
    double Duration = .25;
    uint64 LastShotFrame = MAX_uint64;
    bool bActive = false;
    void Reset();
    bool Trigger(const UCandyFightWeaponConfig& Config, FRandomStream& Random, double Now, uint64 Frame, const FVector2D& RecoilDirection);
    static FVector2D ProjectRecoil(const FVector& ShotDirection, const FQuat& CameraRotation);
    void Evaluate(double Now, FVector2D& OutOffset, double& OutRoll);
    static FMinimalViewInfo Compose(const FMinimalViewInfo& Base, const FVector2D& Offset, double Roll);
};

/** Runs after combat and after logical aiming; writes only the final camera cache. */
UCLASS(ClassGroup=(CandyFight), meta=(BlueprintSpawnableComponent))
class TPSDEMO_API UCandyFightFireCameraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCandyFightFireCameraComponent();
    void NotifyShot(const UCandyFightWeaponConfig& Config, double Now, const FVector& ShotDirection);
    void RecordLogicalCamera(uint64 Frame, const FRotator& Rotation);
    void ApplyVisualCamera(double Now, uint64 Frame);
    UFUNCTION(BlueprintCallable, Category="Camera Shake") void ClearFeedback();
    UPROPERTY(Transient, BlueprintReadOnly, Category="Camera Shake") FVector2D CurrentOffset = FVector2D::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Camera Shake") float CurrentRoll = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Camera Shake") int32 ShakeRequests = 0;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    friend class FCandyFightFireCameraIntegrationTest;
    FCandyFightFireCameraState State;
    FRandomStream Random;
    FQuat LogicalRotation = FQuat::Identity;
    uint64 LogicalFrame = MAX_uint64;
    uint64 Revision = 0;
    uint64 AppliedRevision = MAX_uint64;
    bool bWroteCamera = false;
    TWeakObjectPtr<APlayerCameraManager> WrittenManager;
    FMinimalViewInfo BaseView;
    FMinimalViewInfo WrittenView;
};
