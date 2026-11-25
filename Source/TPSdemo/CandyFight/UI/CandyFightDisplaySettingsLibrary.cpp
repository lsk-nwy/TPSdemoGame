#include "CandyFight/UI/CandyFightDisplaySettingsLibrary.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

bool UCandyFightDisplaySettingsLibrary::ApplyFramePacingSettings(float FrameRateLimit,
	bool bEnableVSync, bool bSaveSettings)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return false;
	}

	const float SafeLimit = FMath::IsFinite(FrameRateLimit) ? FMath::Max(0.0f, FrameRateLimit) : 0.0f;
	Settings->SetFrameRateLimit(SafeLimit);
	Settings->SetVSyncEnabled(bEnableVSync);
	Settings->ApplyNonResolutionSettings();
	if (bSaveSettings)
	{
		Settings->SaveSettings();
	}
	return true;
}

bool UCandyFightDisplaySettingsLibrary::GetFramePacingSettings(float& FrameRateLimit, bool& bVSyncEnabled)
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		FrameRateLimit = 0.0f;
		bVSyncEnabled = false;
		return false;
	}

	FrameRateLimit = Settings->GetFrameRateLimit();
	bVSyncEnabled = Settings->IsVSyncEnabled();
	return true;
}
