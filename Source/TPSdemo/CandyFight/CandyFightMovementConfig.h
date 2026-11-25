#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CandyFightMovementConfig.generated.h"

// 保存玩家移动、摇杆死区和碰撞胶囊的可调参数。
UCLASS(BlueprintType)
class TPSDEMO_API UCandyFightMovementConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1", Units="cm/s"))
	float MaxWalkSpeed = 600.0f; // 最大地面移速，单位 cm/s。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1", Units="cm/s^2"))
	float MaxAcceleration = 6000.0f; // 加速度，越大越快达到目标速度，单位 cm/s²。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1", Units="cm/s^2"))
	float BrakingDeceleration = 6000.0f; // 松开输入后的制动减速度，单位 cm/s²。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0"))
	float GroundFriction = 8.0f; // 地面摩擦，影响转向响应和制动。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1", Units="deg/s"))
	float TurnRate = 720.0f; // 朝移动方向转身的速度，单位 °/s。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(ClampMin="0", ClampMax="0.95"))
	float GamepadDeadZone = 0.15f; // 忽略低于此幅度的摇杆输入，防止漂移。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capsule", meta=(ClampMin="1", Units="cm"))
	float CapsuleRadius = 42.0f; // 碰撞胶囊半径，单位 cm。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capsule", meta=(ClampMin="1", Units="cm"))
	float CapsuleHalfHeight = 88.0f; // 含两端圆顶的总高度的一半，不能小于半径，单位 cm。

	bool ValidateConfiguration(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
