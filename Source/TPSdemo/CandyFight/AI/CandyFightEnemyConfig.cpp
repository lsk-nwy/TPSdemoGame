#include "CandyFightEnemyConfig.h"

bool UCandyFightEnemyConfig::IsValidConfiguration() const
{
	const float Values[] = {WalkSpeed, AcceptanceRadius, WaitMin, WaitMax, RandomRadius,
		RetryDelay, StuckTimeout, MoveTimeout, ProjectionDistance};
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f) return false;
	}
	return WaitMax >= WaitMin && MaxPointFailures >= 1 && MaxPointFailures <= 10;
}

bool UCandyFightEnemyConfig::IsValidWeaponDriver() const
{
    return BurstShotCount > 0 && FMath::IsFinite(BurstPauseSeconds) && BurstPauseSeconds >= 0.f;
}

bool UCandyFightEnemyConfig::IsValidAutonomy() const
{
    const float Positive[] = {SightDistance, SightAngleDegrees, HearingDistance, PerceptionInterval,
        LeashRadius, ClueTimeout, CombatStuckTimeout, CombatWalkSpeed, RifleMinDistance, RifleMaxDistance,
        ShotgunMinDistance, ShotgunMaxDistance, PositionInterval, DestinationTolerance, SearchSeconds,
        SearchRadius, SearchTurnSpeed, ReturnRetrySeconds, ReturnFallbackRadius};
    for (float V : Positive) if (!FMath::IsFinite(V) || V <= 0.f) return false;
    const float Nonnegative[] = {SightHeight, DistanceHysteresis, PositionRetainTolerance, MaxFireSpeed};
    for (float V : Nonnegative) if (!FMath::IsFinite(V) || V < 0.f) return false;
    return SightAngleDegrees <= 360.f && RifleMinDistance < RifleMaxDistance &&
        ShotgunMinDistance < ShotgunMaxDistance && SearchPointCount >= 0 && SearchPointCount <= 8 && ReturnFailuresBeforeFallback > 0;
}
