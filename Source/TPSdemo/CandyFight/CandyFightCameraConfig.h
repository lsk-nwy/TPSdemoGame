#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CandyFightCameraConfig.generated.h"

// 保存固定俯视镜头的构图和跟随参数。
UCLASS(BlueprintType)
class TPSDEMO_API UCandyFightCameraConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="1", Units="cm"))
	float Distance = 1600.0f; // 弹簧臂长度，越大镜头越远，单位 cm。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="1", ClampMax="89", Units="deg"))
	float PitchDegrees = 55.0f; // 向下俯视角，应用时转为负 Pitch，单位 °。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(Units="deg"))
	float YawDegrees = 0.0f; // 镜头的水平朝向，也决定移动方向，单位 °。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing", meta=(ClampMin="5", ClampMax="170", Units="deg"))
	float FieldOfView = 50.0f; // 视野角度，越大可见范围越广，单位 °。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Following", meta=(ClampMin="0"))
	float FollowLagSpeed = 8.0f; // 越大跟随越紧；为 0 时关闭平滑延迟。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Following", meta=(ClampMin="0", Units="cm"))
	float MaxLagDistance = 200.0f; // 跟随目标允许落后角色的距离上限，0 表示不限制，单位 cm。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing")
	bool bEnableCollisionTest = false; // 开启后，镜头遇到遮挡会缩短弹簧臂。

	/** 启用后准星向边缘移动时，人物在屏幕中向对侧移动。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming")
	bool bEnableAimScreenOffset = true;

	/** 准星到左右边缘时，人物中心距离对侧边缘的屏幕宽度比例；0.5 保持横向居中。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming", meta=(ClampMin="0", ClampMax="0.5"))
	float AimScreenEdgeMarginX = 0.10f;

	/** 准星到上下边缘时，人物中心距离对侧边缘的屏幕高度比例；0.5 保持纵向居中。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming", meta=(ClampMin="0", ClampMax="0.5"))
	float AimScreenEdgeMarginY = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Deprecated", meta=(DeprecatedProperty, DeprecationMessage="Use AimScreenEdgeMarginX/Y instead."))
	float AimOffsetStrength = 0.35f; // 镜头向准星偏移的比例；0 关闭，1 跟到准星位置。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Deprecated", meta=(DeprecatedProperty, DeprecationMessage="Screen framing no longer uses a world distance cap."))
	float AimMaxOffsetDistance = 250.0f; // 瞄准偏移上限，0 关闭偏移，单位 cm。

	/** 构图平滑速度；0 立即同步，大于 0 时越大越快。不控制角色移动跟随。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming", meta=(ClampMin="0"))
	float AimOffsetInterpSpeed = 0.0f;

	/** Blend away only the framing change caused by rebasing a released crosshair. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming", meta=(ClampMin="0", Units="s"))
	float AimAssistReleaseBlendSeconds = 0.20f;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Migration")
	int32 ScreenFramingVersion = 0;

	bool ValidateConfiguration(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
