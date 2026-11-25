#include "CandyFightFlow.h"
#include "CandyFightFlowWidget.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Interaction/CandyFightSearchComponent.h"
#include "CandyFight/Inventory/CandyFightInventoryComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "InputCoreTypes.h"
#include "TPSdemo.h"

void FCandyFightRunLedger::Start(double Now)
{
    StartedAt = FMath::IsFinite(Now) ? Now : 0.;
    Summary = FCandyFightRunSummary();
    bActive = true;
}

bool FCandyFightRunLedger::Finish(ECandyFightRunOutcome Outcome, int32 Gold, double Now)
{
    if (!bActive || Outcome == ECandyFightRunOutcome::None) return false;
    bActive = false; // 先提交，再显示界面；同一帧死亡、超时、撤离不会重复结算。
    Summary.Outcome = Outcome;
    Summary.Gold = FMath::Max(0, Gold);
    Summary.ElapsedSeconds = FMath::IsFinite(Now) ? float(FMath::Max(0., Now - StartedAt)) : 0.f;
    return true;
}

UCandyFightGameInstance::UCandyFightGameInstance()
{
    FrontendMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/CandyFight/Maps/L_CandyFight_Frontend.L_CandyFight_Frontend")));
    BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/CandyFight/Maps/L_CandyFight_AI_Test.L_CandyFight_AI_Test")));
}

void UCandyFightGameInstance::Init()
{
    Super::Init();
    if (GEngine) GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
}

void UCandyFightGameInstance::Shutdown()
{
    if (GEngine) GEngine->OnTravelFailure().RemoveAll(this);
    ReleaseFlowInput();
    RemoveFlowWidget();
    Super::Shutdown();
}

void UCandyFightGameInstance::RemoveFlowWidget()
{
    if (FlowWidget) FlowWidget->RemoveFromParent();
    FlowWidget = nullptr;
}

void UCandyFightGameInstance::ReleaseFlowInput()
{
    if (InputOwner.IsValid() && FlowInput) InputOwner->PopInputComponent(FlowInput);
    if (FlowInput) FlowInput->DestroyComponent();
    FlowInput = nullptr;
    InputOwner.Reset();
}

void UCandyFightGameInstance::SetScreen(ECandyFightFlowScreen NewScreen)
{
    Screen = NewScreen;
    UE_LOG(LogTPSdemo, Display, TEXT("[Flow] Screen=%s"), *UEnum::GetValueAsString(Screen));
    RemoveFlowWidget();
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->IsLocalController()) return;
    if (PC->PlayerInput) PC->PlayerInput->FlushPressedKeys();
    if (Screen == ECandyFightFlowScreen::Battle)
    {
        FInputModeGameOnly Input;
        Input.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(Input);
        PC->bShowMouseCursor = false; // 战斗使用相对鼠标输入驱动虚拟准星。
        if (auto* Local = PC->GetLocalPlayer(); Local && Local->ViewportClient)
        {
            Local->ViewportClient->SetMouseLockMode(EMouseLockMode::LockAlways);
            Local->ViewportClient->SetHideCursorDuringCapture(true);
        }
        return;
    }
    FlowWidget = CreateWidget<UCandyFightFlowWidget>(PC, UCandyFightFlowWidget::StaticClass());
    if (!FlowWidget) return;
    FlowWidget->Display(Screen, LastResult);
    FlowWidget->AddToViewport(1000);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(FlowWidget->TakeWidget());
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Input);
    PC->bShowMouseCursor = true;
}

void UCandyFightGameInstance::EnterFrontend()
{
    ReleaseFlowInput();
    Run.bActive = false;
    UGameplayStatics::SetGamePaused(this, false);
    SetScreen(Screen == ECandyFightFlowScreen::Preparation ? Screen : ECandyFightFlowScreen::MainMenu);
}

void UCandyFightGameInstance::ShowMainMenu()
{
    if (Run.bActive || Screen == ECandyFightFlowScreen::Loading) return;
    SetScreen(ECandyFightFlowScreen::MainMenu);
}

void UCandyFightGameInstance::ShowPreparation()
{
    if (Run.bActive || Screen == ECandyFightFlowScreen::Loading || Screen == ECandyFightFlowScreen::Settlement) return;
    SetScreen(ECandyFightFlowScreen::Preparation);
}

void UCandyFightGameInstance::OpenPreparationPanel(ECandyFightFlowScreen Panel)
{
    if (Screen != ECandyFightFlowScreen::Preparation) return;
    if (Panel == ECandyFightFlowScreen::Backpack || Panel == ECandyFightFlowScreen::Warehouse ||
        Panel == ECandyFightFlowScreen::Shop || Panel == ECandyFightFlowScreen::Equipment)
        SetScreen(Panel);
}

bool UCandyFightGameInstance::TravelTo(const TSoftObjectPtr<UWorld>& Map)
{
    const FString Package = Map.ToSoftObjectPath().GetLongPackageName();
    if (!FPackageName::DoesPackageExist(Package))
    {
        UE_LOG(LogTPSdemo, Error, TEXT("[Flow] Map not found: %s"), *Package);
        return false;
    }
    UGameplayStatics::OpenLevel(this, FName(*Package));
    return true;
}

void UCandyFightGameInstance::StartBattle()
{
    if (Screen != ECandyFightFlowScreen::Preparation || Run.bActive) return;
    LastResult = FCandyFightRunSummary();
    SetScreen(ECandyFightFlowScreen::Loading);
    if (!TravelTo(BattleMap)) SetScreen(ECandyFightFlowScreen::Preparation);
}

void UCandyFightGameInstance::BeginBattle()
{
    if (Run.bActive || !GetWorld()) return;
    ReleaseFlowInput();
    LastResult = FCandyFightRunSummary();
    Run.Start(GetWorld()->GetTimeSeconds());
    UGameplayStatics::SetGamePaused(this, false);
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->IsLocalController())
    {
        FlowInput = NewObject<UInputComponent>(PC, TEXT("CandyFightFlowInput"));
        FlowInput->RegisterComponent();
        FlowInput->Priority = 10;
        auto& Binding = FlowInput->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ToggleBattleMenu);
        Binding.bExecuteWhenPaused = true;
        auto& TabBinding = FlowInput->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::ToggleBattleMenu);
        TabBinding.bExecuteWhenPaused = true;
        PC->PushInputComponent(FlowInput);
        InputOwner = PC;
    }
    SetScreen(ECandyFightFlowScreen::Battle);
    UE_LOG(LogTPSdemo, Display, TEXT("[Flow] Battle started. Tab / Esc opens the battle menu; B opens the inventory."));
    if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 8.f, FColor::Cyan, TEXT("Tab：战斗菜单 / B：6×6 背包 / E：拾取（独立运行也可按 Esc）"));
}

void UCandyFightGameInstance::ToggleBattleMenu()
{
    if (!Run.bActive) return;
    if (Screen == ECandyFightFlowScreen::Battle)
    {
        if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
            if (auto* Player = Cast<ACandyFightCharacter>(PC->GetPawn()))
            {
                // 背包是战斗内的独立界面；打开暂停菜单前先收起，避免两个界面同时暂停。
                if (Player->Inventory) Player->Inventory->CloseInventory();
                Player->Combat->StopFire();
                Player->Search->CancelSearch();
                Player->ConsumeMovementInputVector();
                Player->GetCharacterMovement()->StopMovementImmediately();
            }
        UGameplayStatics::SetGamePaused(this, true);
        SetScreen(ECandyFightFlowScreen::Pause);
    }
    else if (Screen == ECandyFightFlowScreen::Pause)
    {
        UGameplayStatics::SetGamePaused(this, false);
        SetScreen(ECandyFightFlowScreen::Battle);
    }
}

bool UCandyFightGameInstance::FinishBattle(ECandyFightRunOutcome Outcome)
{
    if (!GetWorld()) return false;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    auto* State = PC ? PC->GetPlayerState<ACandyFightPlayerState>() : nullptr;
    if (!Run.Finish(Outcome, State ? State->Gold : 0, GetWorld()->GetTimeSeconds())) return false;
    LastResult = Run.Summary;
    // 停止枪声和在途子弹，然后冻结战斗世界；结算 UI 仍可操作。
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
        if (auto* Combat = It->FindComponentByClass<UCandyFightCombatComponent>()) Combat->ShutdownCombat();
    if (auto* Player = PC ? Cast<ACandyFightCharacter>(PC->GetPawn()) : nullptr)
    {
        if (Player->Inventory) Player->Inventory->CloseInventory();
        Player->Search->CancelSearch();
    }
    UGameplayStatics::SetGamePaused(this, true);
    SetScreen(ECandyFightFlowScreen::Settlement);
    UE_LOG(LogTPSdemo, Display, TEXT("[Flow] Result=%s Gold=%d Elapsed=%.2fs (display only; no persistence)"),
        *OutcomeText(LastResult.Outcome), LastResult.Gold, LastResult.ElapsedSeconds);
    return true;
}

void UCandyFightGameInstance::ReturnFromBattle(bool bToMainMenu)
{
    if (Screen != ECandyFightFlowScreen::Settlement) return;
    UGameplayStatics::SetGamePaused(this, false);
    ReleaseFlowInput();
    const auto Destination = bToMainMenu ? ECandyFightFlowScreen::MainMenu : ECandyFightFlowScreen::Preparation;
    SetScreen(ECandyFightFlowScreen::Loading);
    if (TravelTo(FrontendMap)) Screen = Destination;
    else
    {
        UGameplayStatics::SetGamePaused(this, true);
        SetScreen(ECandyFightFlowScreen::Settlement);
    }
}

void UCandyFightGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
    if (World && World->GetGameInstance() != this) return;
    UE_LOG(LogTPSdemo, Error, TEXT("[Flow] Travel failed: %s"), *Error);
    Run.bActive = false;
    UGameplayStatics::SetGamePaused(this, false);
    SetScreen(ECandyFightFlowScreen::Preparation);
}

FString UCandyFightGameInstance::OutcomeText(ECandyFightRunOutcome Outcome)
{
    switch (Outcome)
    {
    case ECandyFightRunOutcome::Escaped: return TEXT("撤离成功");
    case ECandyFightRunOutcome::Defeated: return TEXT("撤离失败：角色阵亡");
    case ECandyFightRunOutcome::TimedOut: return TEXT("撤离失败：战斗超时");
    case ECandyFightRunOutcome::Abandoned: return TEXT("撤离失败：主动放弃");
    default: return TEXT("尚无结算");
    }
}

ACandyFightFrontendGameMode::ACandyFightFrontendGameMode()
{
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;
}

void ACandyFightFrontendGameMode::StartPlay()
{
    Super::StartPlay();
    if (auto* Flow = Cast<UCandyFightGameInstance>(GetGameInstance())) Flow->EnterFrontend();
}