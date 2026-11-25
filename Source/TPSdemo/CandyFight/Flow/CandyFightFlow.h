#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/GameModeBase.h"
#include "CandyFightFlow.generated.h"

class UCandyFightFlowWidget;
class UInputComponent;

UENUM(BlueprintType)
enum class ECandyFightFlowScreen : uint8
{
    MainMenu, Preparation, Backpack, Warehouse, Shop, Equipment, Loading, Battle, Pause, Settlement
};

UENUM(BlueprintType)
enum class ECandyFightRunOutcome : uint8 { None, Escaped, Defeated, TimedOut, Abandoned };

USTRUCT(BlueprintType)
struct TPSDEMO_API FCandyFightRunSummary
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ECandyFightRunOutcome Outcome = ECandyFightRunOutcome::None;
    UPROPERTY(BlueprintReadOnly) int32 Gold = 0;
    UPROPERTY(BlueprintReadOnly) float ElapsedSeconds = 0.f;
};

// 一局只提交一次结果。独立于 UI 和关卡，方便验证重复回调及再次开局。
struct TPSDEMO_API FCandyFightRunLedger
{
    bool bActive = false;
    double StartedAt = 0.;
    FCandyFightRunSummary Summary;
    void Start(double Now);
    bool Finish(ECandyFightRunOutcome Outcome, int32 Gold, double Now);
};

// GameInstance 在 OpenLevel 时保留；角色、敌人、金币则随战斗地图重新创建。
UCLASS()
class TPSDEMO_API UCandyFightGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    UCandyFightGameInstance();
    virtual void Init() override;
    virtual void Shutdown() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow") TSoftObjectPtr<UWorld> FrontendMap;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow") TSoftObjectPtr<UWorld> BattleMap;
    UPROPERTY(BlueprintReadOnly, Category="Flow") ECandyFightFlowScreen Screen = ECandyFightFlowScreen::MainMenu;
    UPROPERTY(BlueprintReadOnly, Category="Flow") FCandyFightRunSummary LastResult;

    UFUNCTION(BlueprintCallable, Category="Flow") void ShowMainMenu();
    UFUNCTION(BlueprintCallable, Category="Flow") void ShowPreparation();
    UFUNCTION(BlueprintCallable, Category="Flow") void OpenPreparationPanel(ECandyFightFlowScreen Panel);
    UFUNCTION(BlueprintCallable, Category="Flow") void StartBattle();
    UFUNCTION(BlueprintCallable, Category="Flow") void ToggleBattleMenu();
    UFUNCTION(BlueprintCallable, Category="Flow") bool FinishBattle(ECandyFightRunOutcome Outcome);
    UFUNCTION(BlueprintCallable, Category="Flow") void ReturnFromBattle(bool bToMainMenu = false);
    UFUNCTION(BlueprintPure, Category="Flow") bool IsBattleActive() const { return Run.bActive; }

    // 由两种 GameMode 调用，入口集中，界面不直接创建玩家或敌人。
    void EnterFrontend();
    void BeginBattle();
    static FString OutcomeText(ECandyFightRunOutcome Outcome);

private:
    FCandyFightRunLedger Run;
    UPROPERTY(Transient) TObjectPtr<UCandyFightFlowWidget> FlowWidget;
    UPROPERTY(Transient) TObjectPtr<UInputComponent> FlowInput;
    TWeakObjectPtr<APlayerController> InputOwner;
    void SetScreen(ECandyFightFlowScreen NewScreen);
    void RemoveFlowWidget();
    void ReleaseFlowInput();
    bool TravelTo(const TSoftObjectPtr<UWorld>& Map);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
};

// 菜单关卡不生成角色，不启动敌人或战斗计时。
UCLASS()
class TPSDEMO_API ACandyFightFrontendGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACandyFightFrontendGameMode();
    virtual void StartPlay() override;
};