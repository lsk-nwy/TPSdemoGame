#include "CandyFight/CandyFightCameraConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// 检查镜头距离、角度和平滑参数是否有效。
bool UCandyFightCameraConfig::ValidateConfiguration(FString& OutError) const
{
	OutError.Reset();
	if (!FMath::IsFinite(Distance) || Distance <= 0.0f ||
		!FMath::IsFinite(PitchDegrees) || PitchDegrees < 1.0f || PitchDegrees > 89.0f ||
		!FMath::IsFinite(YawDegrees) ||
		!FMath::IsFinite(FieldOfView) || FieldOfView < 5.0f || FieldOfView > 170.0f)
	{
		OutError = TEXT("Camera framing requires a positive distance, pitch in [1, 89], FOV in [5, 170], and finite values.");
	}
	else if (!FMath::IsFinite(FollowLagSpeed) || FollowLagSpeed < 0.0f ||
		!FMath::IsFinite(MaxLagDistance) || MaxLagDistance < 0.0f)
	{
		OutError = TEXT("Camera lag speed and maximum lag distance must be finite and nonnegative; zero lag speed disables smoothing.");
	}
	else if (!FMath::IsFinite(AimScreenEdgeMarginX) || AimScreenEdgeMarginX < 0.0f || AimScreenEdgeMarginX > 0.5f ||
		!FMath::IsFinite(AimScreenEdgeMarginY) || AimScreenEdgeMarginY < 0.0f || AimScreenEdgeMarginY > 0.5f ||
		!FMath::IsFinite(AimOffsetInterpSpeed) || AimOffsetInterpSpeed < 0.0f)
	{
		OutError = TEXT("瞄准构图留边比例必须在 [0, 0.5] 内；平滑速度必须为有限非负值，0 表示立即同步。");
	}
	else if (!FMath::IsFinite(AimAssistReleaseBlendSeconds) || AimAssistReleaseBlendSeconds < 0.0f)
	{
		OutError = TEXT("Aim assist release camera blend time must be finite and nonnegative; zero disables the blend.");
	}
	return OutError.IsEmpty();
}

#if WITH_EDITOR
// 在编辑器资产校验中显示无效参数。
EDataValidationResult UCandyFightCameraConfig::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateConfiguration(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
