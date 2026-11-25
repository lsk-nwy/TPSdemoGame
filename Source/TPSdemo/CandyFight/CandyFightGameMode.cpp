#include "CandyFight/CandyFightGameMode.h"
#include "CandyFight/Flow/CandyFightFlow.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/Inventory/CandyFightItem.h"
#include "CandyFight/Inventory/CandyFightItemPickup.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TPSdemo.h"

ACandyFightGameMode::ACandyFightGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	// C++ 提供基础角色，关卡使用的 GameMode 蓝图将其覆盖为已配置的角色蓝图。
	DefaultPawnClass = ACandyFightCharacter::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass(); // 使用基础玩家控制器。
	PlayerStateClass = ACandyFightPlayerState::StaticClass();
	TestPickupItems = UCandyFightItemLibrary::GetTestItemIds();
	TestPickupClass = ACandyFightItemPickup::StaticClass();
}

void ACandyFightGameMode::StartPlay()
{
	if (!bCountdownStarted)
	{
		const float Duration = FMath::IsFinite(CountdownDuration) ? FMath::Max(0.f, CountdownDuration) : 0.f;
		CountdownEndTime = GetWorld()->GetTimeSeconds() + double(Duration);
		bCountdownStarted = true;
	}
	Super::StartPlay();
	if (auto* Flow = Cast<UCandyFightGameInstance>(GetGameInstance())) Flow->BeginBattle();
	if (bSpawnTestPickups && !bTestPickupsSpawned && GetWorld())
	{
		// 玩家可能在 StartPlay 之后才完成生成；延迟一点并允许重试。
		GetWorldTimerManager().SetTimer(TestPickupTimer, this, &ThisClass::SpawnTestPickups, .35f, false);
	}
}

void ACandyFightGameMode::SpawnTestPickups()
{
	if (bTestPickupsSpawned || !GetWorld() || !GetWorld()->IsGameWorld()) return;
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		if (++TestPickupAttempts < 8)
		{
			GetWorldTimerManager().SetTimer(TestPickupTimer, this, &ThisClass::SpawnTestPickups, .25f, false);
		}
		return;
	}

	bTestPickupsSpawned = true;
	TArray<FName> Items = TestPickupItems;
	if (Items.Num() == 0) Items = UCandyFightItemLibrary::GetTestItemIds();
	const float Radius = FMath::IsFinite(TestPickupRadius) ? FMath::Max(0.f, TestPickupRadius) : 0.f;
	const FRotator Facing(0.f, Player->GetActorRotation().Yaw, 0.f);
	UWorld* World = GetWorld();
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const float Alpha = Items.Num() > 1 ? float(Index) / float(Items.Num() - 1) : .5f;
		const float Angle = FMath::DegreesToRadians(FMath::Lerp(-72.f, 72.f, Alpha));
		const FVector Direction = Facing.RotateVector(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f));
		const FVector Target = Player->GetActorLocation() + Direction * Radius;
		FVector Location = Target;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightTestPickup), false, Player);
		if (World->LineTraceSingleByChannel(Hit, Target + FVector(0.f, 0.f, 120.f),
			Target - FVector(0.f, 0.f, 320.f), ECC_Visibility, Params))
		{
			Location = Hit.ImpactPoint + FVector(0.f, 0.f, 35.f);
		}
		else
		{
			Location.Z -= 60.f;
		}
		if (ACandyFightItemPickup::SpawnItem(World, Items[Index], Location,
			TestPickupClass, this, .4f))
		{
			++Spawned;
		}
	}
	UE_LOG(LogTPSdemo, Display, TEXT("[Inventory] Spawned %d test pickups around the player."), Spawned);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 10.f, FColor::Green,
			TEXT("测试道具：靠近后按 E 拾取；按 B 打开 6×6 背包，ESC 关闭。"));
	}
}

float ACandyFightGameMode::GetRemainingCountdownSeconds() const
{
	if (!bCountdownStarted)
		return FMath::IsFinite(CountdownDuration) ? FMath::Max(0.f, CountdownDuration) : 0.f;
	if (bCountdownFinished || !GetWorld()) return 0.f;
	return float(FMath::Max(0., CountdownEndTime - GetWorld()->GetTimeSeconds()));
}

void ACandyFightGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCountdownStarted && !bCountdownFinished && GetRemainingCountdownSeconds() <= 0.f)
	{
		bCountdownFinished = true;
		SetActorTickEnabled(false);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTPSdemo,Display,TEXT("本局时间已结束"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE,5.f,FColor::Yellow,TEXT("本局时间已结束"));
#endif
		OnCountdownFinished.Broadcast();
		if (auto* Flow = Cast<UCandyFightGameInstance>(GetGameInstance())) Flow->FinishBattle(ECandyFightRunOutcome::TimedOut);
	}
}
