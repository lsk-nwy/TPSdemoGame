#include "CandyFightSearchComponent.h"
#include "CandyFightChest.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"

UCandyFightSearchComponent::UCandyFightSearchComponent() { PrimaryComponentTick.bCanEverTick = true; }
void UCandyFightSearchComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner())) Player->OnPlayerDamaged.AddUniqueDynamic(this,&ThisClass::HandleDamage);
    if (FSlateApplication::IsInitialized()) ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&ThisClass::OnActivationChanged);
    InitializeLocalPlayer();
}
void UCandyFightSearchComponent::InitializeLocalPlayer()
{
    RemoveMapping();
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    auto* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer() || !InteractMapping || !InteractAction) return;
    if (auto* Subsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        RegisteredMapping = DuplicateObject<UInputMappingContext>(InteractMapping,this);
        Subsystem->AddMappingContext(RegisteredMapping,2); InputSubsystem = Subsystem;
    }
}
void UCandyFightSearchComponent::RemoveMapping()
{
    if (auto* Subsystem = InputSubsystem.Get(); Subsystem && RegisteredMapping) Subsystem->RemoveMappingContext(RegisteredMapping);
    RegisteredMapping = nullptr; InputSubsystem.Reset();
}
void UCandyFightSearchComponent::BindInput(UEnhancedInputComponent* Input)
{
    if (!Input || !InteractAction) return;
    Input->BindAction(InteractAction,ETriggerEvent::Started,this,&ThisClass::HandlePress);
    Input->BindAction(InteractAction,ETriggerEvent::Completed,this,&ThisClass::HandleRelease);
    Input->BindAction(InteractAction,ETriggerEvent::Canceled,this,&ThisClass::HandleRelease);
}
void UCandyFightSearchComponent::HandlePress(const FInputActionValue& Value)
{
    if (TryBeginSearch()) bCheckPhysicalInput = true;
}
void UCandyFightSearchComponent::HandleRelease(const FInputActionValue& Value) { CancelSearch(); }
void UCandyFightSearchComponent::OnActivationChanged(bool bActive) { if (!bActive) CancelSearch(); }
void UCandyFightSearchComponent::HandleDamage(float AppliedDamage)
{
    if (AppliedDamage > 0 && bSearching) { if (IsValid(Target)) Target->ShowInterrupted(); CancelSearch(); }
}
FText UCandyFightSearchComponent::GetInteractKeyText() const
{
    if (InteractMapping) for (const auto& Mapping : InteractMapping->GetMappings())
        if (Mapping.Action == InteractAction) return Mapping.Key.GetDisplayName();
    return FText::FromString(TEXT("E"));
}
void UCandyFightSearchComponent::RefreshTarget()
{
    if (bSearching) return;
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    ACandyFightChest* Nearest = nullptr; double Best = TNumericLimits<double>::Max();
    if (Player && Player->GetController()) for (TActorIterator<ACandyFightChest> It(GetWorld()); It; ++It)
    {
        if (It->State != ECandyFightChestState::Unsearched || !It->IsInReach(Player)) continue;
        const double Distance = FVector::DistSquared2D(Player->GetActorLocation(),It->GetActorLocation());
        if (Distance < Best) { Best = Distance; Nearest = *It; }
    }
    if (Target != Nearest) { Target = Nearest; OnSearchChanged.Broadcast(); }
}
bool UCandyFightSearchComponent::TryBeginSearch()
{
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    if (bSearching || !Player || !Player->GetController() || UGameplayStatics::IsGamePaused(this) ||
        Player->Combat->IsReloading() || !Player->GetPlayerState<ACandyFightPlayerState>()) return false;
    RefreshTarget();
    if (!IsValid(Target) || !Target->TryClaim(Player)) return false;
    SearchDuration = Target->GetConfig()->SearchSeconds; SearchStart = GetWorld()->GetTimeSeconds();
    bSearching = true; bCheckPhysicalInput = false;
    Player->ConsumeMovementInputVector(); Player->GetCharacterMovement()->StopMovementImmediately();
    Player->Combat->SuspendForSearch(); OnSearchChanged.Broadcast(); return true;
}
float UCandyFightSearchComponent::GetSearchProgress() const
{
    return bSearching && GetWorld() && SearchDuration > 0 ? FMath::Clamp(float((GetWorld()->GetTimeSeconds()-SearchStart)/SearchDuration),0.f,1.f) : 0.f;
}
void UCandyFightSearchComponent::FinishSearchState()
{
    bSearching = false; bCheckPhysicalInput = false; SearchStart = 0.;
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner())) Player->Combat->SuspendForSearch();
    NextTargetRefresh = 0.; OnSearchChanged.Broadcast();
}
void UCandyFightSearchComponent::CancelSearch()
{
    if (!bSearching) return;
    if (IsValid(Target)) Target->ReleaseClaim(Cast<ACandyFightCharacter>(GetOwner()));
    FinishSearchState();
}
void UCandyFightSearchComponent::UpdateSearch()
{
    if (!bSearching) return;
    auto* Player = Cast<ACandyFightCharacter>(GetOwner());
    if (!Player || !Player->GetController() || !IsValid(Target) || !Target->IsInReach(Player)) { CancelSearch(); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (bCheckPhysicalInput)
    {
        auto* PC = Cast<APlayerController>(Player->GetController());
        auto* Input = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
        const auto* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
        const auto* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
        if (!Viewport || !Viewport->Viewport || !Viewport->Viewport->HasFocus()) { CancelSearch(); return; }
        if (!Input || !InteractAction || !Input->GetActionValue(InteractAction).Get<bool>()) { CancelSearch(); return; }
    }
    if (GetSearchProgress() >= 1.f)
    {
        if (!Target->CompleteSearch(Player)) Target->ReleaseClaim(Player);
        FinishSearchState();
    }
}
void UCandyFightSearchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    UpdateSearch();
    if (!bSearching && GetWorld()->GetTimeSeconds() >= NextTargetRefresh)
    {
        RefreshTarget(); NextTargetRefresh = GetWorld()->GetTimeSeconds() + FMath::Max(0.f,TargetRefreshSeconds);
    }
}
void UCandyFightSearchComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelSearch(); RemoveMapping();
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner())) Player->OnPlayerDamaged.RemoveDynamic(this,&ThisClass::HandleDamage);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
    Super::EndPlay(Reason);
}
