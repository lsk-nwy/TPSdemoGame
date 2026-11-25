#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CandyFightDisplaySettingsLibrary.generated.h"

/** Blueprint access to the frame pacing settings stored by UGameUserSettings. */
UCLASS()
class TPSDEMO_API UCandyFightDisplaySettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Applies the frame rate limit and VSync immediately.
	 * A frame rate limit of zero disables the limit. Saved values are restored on later launches.
	 */
	UFUNCTION(BlueprintCallable, Category="CandyFight|Settings|Display",
		meta=(DisplayName="Apply Frame Rate and VSync", AdvancedDisplay="bSaveSettings"))
	static bool ApplyFramePacingSettings(float FrameRateLimit = 120.0f,
		bool bEnableVSync = false, bool bSaveSettings = true);

	/** Reads the configured values. The effective frame rate can still be limited by VSync or the platform. */
	UFUNCTION(BlueprintPure, Category="CandyFight|Settings|Display",
		meta=(DisplayName="Get Frame Rate and VSync"))
	static bool GetFramePacingSettings(float& FrameRateLimit, bool& bVSyncEnabled);
};
