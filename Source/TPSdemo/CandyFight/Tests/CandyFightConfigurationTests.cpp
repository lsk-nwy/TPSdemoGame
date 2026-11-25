#if WITH_DEV_AUTOMATION_TESTS

#include "CandyFight/CandyFightCameraConfig.h"
#include "CandyFight/CandyFightMovementConfig.h"
#include "Misc/AutomationTest.h"

// 验证默认配置可用，并能拦截非法调参。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCandyFightConfigurationTest, "CandyFight.Configuration.ValidatesTuning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCandyFightConfigurationTest::RunTest(const FString& Parameters)
{
	FString Error;
	UCandyFightMovementConfig* Movement = NewObject<UCandyFightMovementConfig>();
	UCandyFightCameraConfig* Camera = NewObject<UCandyFightCameraConfig>();
	TestTrue(TEXT("Default movement is valid"), Movement->ValidateConfiguration(Error));
	TestTrue(TEXT("Default camera is valid"), Camera->ValidateConfiguration(Error));

	// 覆盖死区、胶囊尺寸和移速的非法情况。
	Movement->GamepadDeadZone = 1.0f;
	TestFalse(TEXT("A dead zone cannot consume the entire stick range"), Movement->ValidateConfiguration(Error));
	Movement->GamepadDeadZone = 0.15f;
	Movement->CapsuleHalfHeight = Movement->CapsuleRadius - 1.0f;
	TestFalse(TEXT("A capsule cannot be shorter than its diameter"), Movement->ValidateConfiguration(Error));
	Movement->CapsuleHalfHeight = 88.0f;
	Movement->MaxWalkSpeed = -1.0f;
	TestFalse(TEXT("Negative speed is invalid"), Movement->ValidateConfiguration(Error));

	// 零跟随速度合法；垂直俯角和零距离超出当前支持范围。
	Camera->FollowLagSpeed = 0.0f;
	TestTrue(TEXT("Zero lag speed supports instantaneous following"), Camera->ValidateConfiguration(Error));
	Camera->PitchDegrees = 90.0f;
	TestFalse(TEXT("A vertical camera is outside the supported tilt range"), Camera->ValidateConfiguration(Error));
	Camera->PitchDegrees = 55.0f;
	Camera->Distance = 0.0f;
	TestFalse(TEXT("A zero camera distance is invalid"), Camera->ValidateConfiguration(Error));
	return true;
}

#endif
