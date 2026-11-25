#include "CandyFight/CandyFightMovementConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// 检查数值范围和胶囊尺寸，返回失败原因供调用方显示。
bool UCandyFightMovementConfig::ValidateConfiguration(FString& OutError) const
{
	OutError.Reset();
	if (!FMath::IsFinite(MaxWalkSpeed) || MaxWalkSpeed <= 0.0f ||
		!FMath::IsFinite(MaxAcceleration) || MaxAcceleration <= 0.0f ||
		!FMath::IsFinite(BrakingDeceleration) || BrakingDeceleration <= 0.0f ||
		!FMath::IsFinite(GroundFriction) || GroundFriction < 0.0f ||
		!FMath::IsFinite(TurnRate) || TurnRate <= 0.0f)
	{
		OutError = TEXT("Movement speed, acceleration, braking and turn rate must be finite and positive; friction must be nonnegative.");
	}
	else if (!FMath::IsFinite(GamepadDeadZone) || GamepadDeadZone < 0.0f || GamepadDeadZone >= 1.0f)
	{
		OutError = TEXT("Gamepad dead zone must be finite and in [0, 1).");
	}
	else if (!FMath::IsFinite(CapsuleRadius) || CapsuleRadius <= 0.0f ||
		!FMath::IsFinite(CapsuleHalfHeight) || CapsuleHalfHeight < CapsuleRadius)
	{
		OutError = TEXT("Capsule radius must be positive and half height must be at least the radius.");
	}
	return OutError.IsEmpty();
}

#if WITH_EDITOR
// 将相同检查接入编辑器的资产校验。
EDataValidationResult UCandyFightMovementConfig::IsDataValid(FDataValidationContext& Context) const
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
