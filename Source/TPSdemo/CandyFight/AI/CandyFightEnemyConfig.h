#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CandyFightEnemyConfig.generated.h"

/** Shared tuning only. All patrol progress lives on the individual controller. */
UCLASS(BlueprintType)
class TPSDEMO_API UCandyFightEnemyConfig : public UDataAsset
{
	GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="1"))
    float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    TObjectPtr<class UCandyFightWeaponConfig> DefaultWeapon;
    // 一组按实际开火次数计数，霰弹的一组弹丸算一枪。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1"))
    int32 BurstShotCount = 5;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0", Units="s"))
    float BurstPauseSeconds = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    TObjectPtr<class USoundAttenuation> WeaponAttenuation;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Weapon")
    int32 WeaponMigrationVersion = 0;
    bool IsValidWeaponDriver() const;
    // 感知只产生线索；归位途中也可重新追踪，但不读取被遮挡玩家的当前位置。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy", meta=(ToolTip="启用视觉、枪声调查、战斗寻位与归位"))
    bool bEnableAutonomousCombat = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy")
    TObjectPtr<class UBehaviorTree> AutonomyTree;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Autonomy") int32 AutonomyMigrationVersion = 0;
    bool IsValidAutonomy() const;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Perception", meta=(ClampMin="0.1", ToolTip="最大水平视距，厘米"))
    float SightDistance = 1500.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Perception", meta=(ClampMin="0.1", ToolTip="水平视野总夹角，度"))
    float SightAngleDegrees = 120.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Perception", meta=(ClampMin="0", ToolTip="观察点相对胶囊中心高度，厘米"))
    float SightHeight = 60.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Perception", meta=(ClampMin="0.1", ToolTip="玩家成功开枪的听觉半径，可穿墙，厘米"))
    float HearingDistance = 2000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Perception", meta=(ClampMin="0.01", ToolTip="感知检查间隔，秒"))
    float PerceptionInterval = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Pursuit", meta=(ClampMin="1", ToolTip="以首次离开巡逻的位置为中心限制路径及目的地"))
    float LeashRadius = 2500.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Pursuit", meta=(ClampMin="0.1", ToolTip="无新视觉或枪声线索的最长追踪时间"))
    float ClueTimeout = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Pursuit", meta=(ClampMin="0.1", ToolTip="没有有效战斗位置且移动无进展时放弃"))
    float CombatStuckTimeout = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Pursuit", meta=(ClampMin="1", ToolTip="战斗、调查、归位移动速度"))
    float CombatWalkSpeed = 350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="1", ToolTip="半自动和全自动停射距离下界"))
    float RifleMinDistance = 600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="1", ToolTip="半自动和全自动停射距离上界"))
    float RifleMaxDistance = 1000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="1", ToolTip="霰弹停射距离下界"))
    float ShotgunMinDistance = 300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="1", ToolTip="霰弹停射距离上界"))
    float ShotgunMaxDistance = 600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="0", ToolTip="已开始射击时的走停距离缓冲"))
    float DistanceHysteresis = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="0.01", ToolTip="寻位重新评估间隔"))
    float PositionInterval = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="1", ToolTip="目的地到达和重复请求容差"))
    float DestinationTolerance = 75.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="0", ToolTip="新位置评分改善不足此值时保留原目标"))
    float PositionRetainTolerance = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Positioning", meta=(ClampMin="0", ToolTip="允许射击的最大水平速度"))
    float MaxFireSpeed = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Search", meta=(ClampMin="0.1", ToolTip="到达线索后移动搜索时长"))
    float SearchSeconds = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Search", meta=(ClampMin="1", ToolTip="最后已知位置周围搜索半径"))
    float SearchRadius = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Search", meta=(ClampMin="1", ToolTip="无可达搜索点时原地观察转速"))
    float SearchTurnSpeed = 90.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Return", meta=(ClampMin="0.1", ToolTip="归位失败重试间隔"))
    float ReturnRetrySeconds = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Return", meta=(ClampMin="1", ToolTip="归位点不可达时备选点半径"))
    float ReturnFallbackRadius = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Search", meta=(ClampMin="0", ClampMax="8", ToolTip="一次搜索最多访问的点数")) int32 SearchPointCount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Autonomy|Return", meta=(ClampMin="1")) int32 ReturnFailuresBeforeFallback = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="1"))
	float WalkSpeed = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="1"))
	float AcceptanceRadius = 75.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0.1"))
	float WaitMin = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0.1"))
	float WaitMax = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="1"))
	float RandomRadius = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recovery", meta=(ClampMin="0.1"))
	float RetryDelay = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recovery", meta=(ClampMin="1", ClampMax="10"))
	int32 MaxPointFailures = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recovery", meta=(ClampMin="1"))
	float StuckTimeout = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recovery", meta=(ClampMin="1"))
	float MoveTimeout = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="1"))
	float ProjectionDistance = 100.0f;

	bool IsValidConfiguration() const;
};
