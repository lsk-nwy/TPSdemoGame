#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"
#include "CandyFightGameMode.generated.h"

class ACandyFightItemPickup;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCandyFightCountdownFinished);

// 关卡游戏规则入口：指定玩家角色和控制器类型。
UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACandyFightGameMode();

	/** Game-time seconds, read once when this level starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="CandyFight|Countdown", meta=(ClampMin="0", Units="s"))
	float CountdownDuration = 240.f;

	UFUNCTION(BlueprintPure, Category="CandyFight|Countdown")
	float GetRemainingCountdownSeconds() const;

	UPROPERTY(BlueprintAssignable, Category="CandyFight|Countdown")
	FCandyFightCountdownFinished OnCountdownFinished;

	/** 是否在本局开始时在玩家周围放置演示物品。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Test Pickups")
	bool bSpawnTestPickups = true;

	/** 演示物品 Id 列表；默认取物品目录中的全部物品。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Test Pickups")
	TArray<FName> TestPickupItems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Test Pickups", meta=(ClampMin="0", Units="cm"))
	float TestPickupRadius = 280.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CandyFight|Test Pickups")
	TSubclassOf<ACandyFightItemPickup> TestPickupClass;

	/** 玩家出生后调用；也会在延迟计时器中重试，直到玩家可用。 */
	UFUNCTION(BlueprintCallable, Category="CandyFight|Test Pickups")
	void SpawnTestPickups();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	double CountdownEndTime = 0.;
	bool bCountdownStarted = false;
	bool bCountdownFinished = false;
	bool bTestPickupsSpawned = false;
	int32 TestPickupAttempts = 0;
	FTimerHandle TestPickupTimer;
};
