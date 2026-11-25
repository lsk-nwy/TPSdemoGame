#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/AI/CandyFightEnemyPerceptionComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightCombatOwner.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/Interaction/CandyFightSearchComponent.h"
#include "CandyFight/CandyFightFireCameraComponent.h"
#include "CandyFight/UI/CandyFightWeaponHUD.h"
#include "CandyFight/UI/CandyFightCrosshairWidget.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TPSdemo.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"

UCandyFightCombatComponent::UCandyFightCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCandyFightCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!IsExternallyDriven() && FSlateApplication::IsInitialized())
        ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::OnApplicationActivationChanged);
    if (const auto* Owner = Cast<ICandyFightCombatOwner>(GetOwner()); Owner && Owner->IsExternalCombatDriver())
    {
        WeaponConfigs.Reset();
        if (auto* Config = Owner->GetInitialCombatWeapon()) WeaponConfigs.Add(Config);
    }
    Random.GenerateNewSeed();
    // AI 的枪不带元素；本地玩家的枪从火元素开始，Q 键循环。
    CurrentElement = IsExternallyDriven() ? ECandyFightElement::None : ECandyFightElement::Fire;
    WeaponStates.SetNum(WeaponConfigs.Num());
    for (int32 I = 0; I < WeaponConfigs.Num(); ++I)
        if (WeaponConfigs[I]) WeaponStates[I].Initialize(*WeaponConfigs[I], GetWorld()->GetTimeSeconds());
    if (!WeaponConfigs.IsEmpty()) EquipTestWeapon(1);
    if (!IsExternallyDriven()) InitializeLocalPlayer();
}

UCandyFightWeaponConfig* UCandyFightCombatComponent::GetCurrentConfig() const
{
    return WeaponConfigs.IsValidIndex(EquippedIndex) ? WeaponConfigs[EquippedIndex].Get() : nullptr;
}
int32 UCandyFightCombatComponent::GetAmmo() const { return WeaponStates.IsValidIndex(EquippedIndex) ? WeaponStates[EquippedIndex].Ammo : 0; }
bool UCandyFightCombatComponent::IsReloading() const { return WeaponStates.IsValidIndex(EquippedIndex) && WeaponStates[EquippedIndex].bReloading; }
ECandyFightReloadPhase UCandyFightCombatComponent::GetReloadPhase() const
{
    return WeaponStates.IsValidIndex(EquippedIndex) ? WeaponStates[EquippedIndex].ReloadPhase : ECandyFightReloadPhase::Ready;
}
bool UCandyFightCombatComponent::IsRacking() const { return false; } // Legacy Blueprint API.
bool UCandyFightCombatComponent::IsDryFiring() const { return WeaponStates.IsValidIndex(EquippedIndex) && WeaponStates[EquippedIndex].bDryFiring; }
float UCandyFightCombatComponent::GetSpread() const { return WeaponStates.IsValidIndex(EquippedIndex) ? WeaponStates[EquippedIndex].Spread : 0.f; }
float UCandyFightCombatComponent::GetReloadProgress() const
{
    if (!IsReloading()) return 0.f;
    const auto& State = WeaponStates[EquippedIndex];
    return FMath::Clamp(float((GetWorld()->GetTimeSeconds() - State.ReloadStartTime) / (State.ReloadEndTime - State.ReloadStartTime)), 0.f, 1.f);
}

bool UCandyFightCombatComponent::EquipTestWeapon(int32 OneBasedIndex)
{
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner()); Player && Player->Search->bSearching) return false;
    if (bCombatShutdown || !GetWorld()) return false;
    const int32 Index = OneBasedIndex - 1;
    if (!WeaponConfigs.IsValidIndex(Index) || !WeaponStates.IsValidIndex(Index) || !WeaponConfigs[Index]) return false;
    if (Index == EquippedIndex) return true;
    FString Error;
    if (!WeaponConfigs[Index]->ValidateConfiguration(Error))
    {
        UE_LOG(LogTPSdemo, Error, TEXT("Weapon %d: %s"), OneBasedIndex, *Error);
        return false;
    }
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.Instigator = Cast<APawn>(GetOwner());
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACandyFightWeapon* NewWeapon = GetWorld()->SpawnActor<ACandyFightWeapon>(WeaponConfigs[Index]->WeaponClass, GetOwner()->GetActorTransform(), Params);
    if (!NewWeapon || !NewWeapon->ValidateMuzzle())
    {
        UE_LOG(LogTPSdemo, Error, TEXT("Weapon %d has no valid registered Muzzle component."), OneBasedIndex);
        if (NewWeapon) NewWeapon->Destroy();
        return false;
    }
    StopWeaponAudio();
    if (auto* Character = Cast<ACandyFightCharacter>(GetOwner())) Character->FireCameraFeedback->ClearFeedback();
    FireIntent.Reset();
    bFireNeedsRelease = true;
    if (WeaponStates.IsValidIndex(EquippedIndex)) WeaponStates[EquippedIndex].CancelReload();
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner())) Player->ResetAimAssist();
    if (EquippedWeapon) EquippedWeapon->Destroy();
    EquippedWeapon = NewWeapon;
    EquippedIndex = Index;
    ApplyElementTint();
    ObservedPhaseSerial = INDEX_NONE;
    const auto* Owner = Cast<ICandyFightCombatOwner>(GetOwner());
    NewWeapon->AttachToComponent(Owner ? Owner->GetCombatMount() : GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    NewWeapon->SetActorRelativeTransform(NewWeapon->MountTransform);
    WeaponStates[Index].ResumeEquipped(*WeaponConfigs[Index], GetWorld()->GetTimeSeconds());
    SyncStateEvents();
    bReportedInvalidConfig = false;
    OnWeaponChanged.Broadcast();
    OnAmmoChanged.Broadcast();
    OnReloadChanged.Broadcast();
    return true;
}

bool UCandyFightCombatComponent::IsExternallyDriven() const
{
    const auto* Owner = Cast<ICandyFightCombatOwner>(GetOwner());
    return Owner && Owner->IsExternalCombatDriver();
}

bool UCandyFightCombatComponent::IsCombatInputAllowed() const
{
    if (const auto* Player = Cast<ACandyFightCharacter>(GetOwner()); Player && Player->Search->bSearching) return false;
    if (bCombatShutdown || !GetWorld() || GetWorld()->bIsTearingDown) return false;
    if (IsExternallyDriven())
    {
        const auto* Pawn = Cast<APawn>(GetOwner());
        return Pawn && Pawn->GetController() && !UGameplayStatics::IsGamePaused(this);
    }
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
    const UGameViewportClient* Viewport = LP ? LP->ViewportClient : nullptr;
    return FApp::HasFocus() && FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive() &&
        PC && PC->IsLocalController() && !PC->IsPaused() && Viewport && Viewport->Viewport && Viewport->Viewport->HasFocus()
        && (!Viewport->ViewportConsole || Viewport->ViewportConsole->ConsoleState.IsNone());
}

bool UCandyFightCombatComponent::SetElement(ECandyFightElement NewElement)
{
    if (bCombatShutdown || !CandyFightElements::IsValidElement(NewElement)) return false;
    if (CurrentElement == NewElement) return true;
    CurrentElement = NewElement;
    ApplyElementTint();
    OnElementChanged.Broadcast();
    return true;
}

void UCandyFightCombatComponent::CycleElement()
{
    if (bCombatShutdown) return;
    const ECandyFightElement Next = CandyFightElements::GetNextElement(CurrentElement);
    if (!SetElement(Next)) return;
    UE_LOG(LogTPSdemo, Display, TEXT("Element switched to %s"), CandyFightElements::GetElementId(Next));
}

void UCandyFightCombatComponent::HandleElementSwitch()
{
    CycleElement();
}

void UCandyFightCombatComponent::ApplyElementTint()
{
    ElementMaterialInstances.Reset();
    if (!IsValid(EquippedWeapon)) return;
    const FLinearColor Color = CandyFightElements::GetElementColor(CurrentElement);
    for (UStaticMeshComponent* Part : {EquippedWeapon->Body.Get(), EquippedWeapon->Barrel.Get(), EquippedWeapon->Grip.Get()})
    {
        if (!Part) continue;
        UMaterialInstanceDynamic* Instance = Part->CreateDynamicMaterialInstance(0);
        if (!Instance) continue;
        // M_WeaponSolid 暴露 Color 向量参数；缺失该参数时设置会被静默忽略。
        Instance->SetVectorParameterValue(TEXT("Color"), Color);
        ElementMaterialInstances.Add(Instance);
    }
}

void UCandyFightCombatComponent::SuspendForSearch()
{
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner())) Player->ResetAimAssist();
    FireIntent.Reset(); bFireNeedsRelease = true;
    if (WeaponStates.IsValidIndex(EquippedIndex)) WeaponStates[EquippedIndex].bDryFiring = false;
    StopAudio(DryAudio); CancelActionAudio(); SyncStateEvents();
}

void UCandyFightCombatComponent::StartFire()
{
    const UCandyFightWeaponConfig* Config = GetCurrentConfig();
    if (bFireNeedsRelease || !IsCombatInputAllowed() || !Config || !WeaponStates.IsValidIndex(EquippedIndex)) return;
    if (IsReloading() && Config->FireMode != ECandyFightFireMode::Shotgun) return;
    FireIntent.Press(WeaponStates[EquippedIndex].CanFire(*Config, GetWorld()->GetTimeSeconds()));
}
void UCandyFightCombatComponent::StopFire()
{
    FireIntent.Release();
    UWorld* World = GetWorld();
    if (bCombatShutdown || !World || World->bIsTearingDown)
    {
        FireIntent.Reset();
        bFireNeedsRelease = true;
        return;
    }
    bFireNeedsRelease = false;
    if (auto* Config = GetCurrentConfig(); Config && WeaponStates.IsValidIndex(EquippedIndex))
    {
        WeaponStates[EquippedIndex].ReleaseTrigger(*Config, World->GetTimeSeconds());
        SyncStateEvents();
    }
}
void UCandyFightCombatComponent::ReleaseFireIntent()
{
    StopFire();
    FireIntent.Reset();
    bFireNeedsRelease = true;
}
void UCandyFightCombatComponent::OnApplicationActivationChanged(bool bActive)
{
    if (!bActive) { ReleaseFireIntent(); StopWeaponAudio(); }
}
void UCandyFightCombatComponent::Reload()
{
    UCandyFightWeaponConfig* Config = GetCurrentConfig();
    if (!IsCombatInputAllowed() || !Config || !WeaponStates.IsValidIndex(EquippedIndex)) return;
    FString Error;
    if (!Config->ValidateConfiguration(Error)) return;
    if (WeaponStates[EquippedIndex].BeginReload(*Config, GetWorld()->GetTimeSeconds()))
    {
        FireIntent.Reset();
        bFireNeedsRelease = true;
        SyncStateEvents();
    }
}
void UCandyFightCombatComponent::HandlePress(const FInputActionValue&) { StartFire(); }
void UCandyFightCombatComponent::HandleRelease(const FInputActionValue&) { StopFire(); }
void UCandyFightCombatComponent::HandleReload(const FInputActionValue&) { Reload(); }

void UCandyFightCombatComponent::BindInput(UEnhancedInputComponent* Input)
{
    if (!Input) return;
    // Q 键动作：优先用配置的资产，否则用运行时创建的 Boolean 动作。
    if (UInputAction* Action = GetElementSwitchAction())
        Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandleElementSwitch);
    if (!FireAction || !ReloadAction) return;
    Input->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::HandlePress);
    Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ThisClass::HandleRelease);
    Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &ThisClass::HandleRelease);
    Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ThisClass::HandleReload);
}

UInputAction* UCandyFightCombatComponent::GetElementSwitchAction()
{
    if (ElementSwitchAction) return ElementSwitchAction.Get();
    if (!RuntimeElementAction)
    {
        RuntimeElementAction = NewObject<UInputAction>(this, TEXT("CandyFightElementSwitchAction"));
        RuntimeElementAction->ValueType = EInputActionValueType::Boolean;
    }
    return RuntimeElementAction.Get();
}

void UCandyFightCombatComponent::InitializeLocalPlayer()
{
    if (bCombatShutdown || IsExternallyDriven()) return;
    RemoveLocalPlayer();
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) return;
    if (CombatMappingContext && FireAction && ReloadAction)
    {
        UEnhancedInputLocalPlayerSubsystem* Subsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        if (Subsystem)
        {
            RegisteredMapping = DuplicateObject<UInputMappingContext>(CombatMappingContext, this);
            // 把 Q 键注入玩家专属映射副本，不改动共享输入资产。
            if (RegisteredMapping)
            {
                if (UInputAction* Action = GetElementSwitchAction()) RegisteredMapping->MapKey(Action, EKeys::Q);
            }
            Subsystem->AddMappingContext(RegisteredMapping, 1);
            InputSubsystem = Subsystem;
        }
    }
    WeaponHUD = CreateWidget<UCandyFightWeaponHUD>(PC, HUDClass ? HUDClass.Get() : UCandyFightWeaponHUD::StaticClass());
    if (WeaponHUD)
    {
        WeaponHUD->Connect(this);
        WeaponHUD->AddToPlayerScreen(50);
    }
}

void UCandyFightCombatComponent::RemoveLocalPlayer()
{
    ReleaseFireIntent();
    StopWeaponAudio();
    if (auto* Subsystem = InputSubsystem.Get()) if (RegisteredMapping) Subsystem->RemoveMappingContext(RegisteredMapping);
    InputSubsystem.Reset();
    RegisteredMapping = nullptr;
    if (WeaponHUD) { WeaponHUD->Connect(nullptr); WeaponHUD->RemoveFromParent(); WeaponHUD = nullptr; }
}

double UCandyFightCombatComponent::GetAimPlaneZ(double Fallback) const
{
    return IsValid(EquippedWeapon) && EquippedWeapon->ValidateMuzzle() ? EquippedWeapon->Muzzle->GetComponentLocation().Z : Fallback;
}

void UCandyFightCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (bCombatShutdown || !GetWorld() || GetWorld()->bIsTearingDown) return;
    const double Now = GetWorld()->GetTimeSeconds();
    UCandyFightWeaponConfig* Config = GetCurrentConfig();
    APawn* Character = Cast<APawn>(GetOwner());
    const auto* Owner = Cast<ICandyFightCombatOwner>(GetOwner());
    if (!Config || !Character || !Owner || !WeaponStates.IsValidIndex(EquippedIndex)) return;
    FVector Aim; uint64 AimFrame = MAX_uint64;
    bool bValidAim = Owner->GetCombatAim(Aim, AimFrame) && !Aim.ContainsNaN();
    FString Error;
    if (!Config->ValidateConfiguration(Error) || !IsValid(EquippedWeapon) || !EquippedWeapon->ValidateMuzzle())
    {
        ReleaseFireIntent();
        StopWeaponAudio();
        if (!bReportedInvalidConfig) UE_LOG(LogTPSdemo, Error, TEXT("Weapon disabled: %s (check Muzzle)."), *Error);
        bReportedInvalidConfig = true;
        return;
    }
    auto& State = WeaponStates[EquippedIndex];
    const int32 PreviousAmmo = State.Ammo;
    const auto PreviousPhase = State.ReloadPhase;
    State.Advance(*Config, Now);
    if (State.Ammo != PreviousAmmo)
    {
        OnAmmoChanged.Broadcast();
        if (PreviousPhase == ECandyFightReloadPhase::Shell) OnShellLoaded.Broadcast();
    }
    SyncStateEvents();
    const auto* SearchingPlayer = Cast<ACandyFightCharacter>(Character);
    if (auto* Player = Cast<ACandyFightCharacter>(Character))
    {
        Player->RevalidateCombatAim();
        bValidAim = Owner->GetCombatAim(Aim, AimFrame) && !Aim.ContainsNaN();
    }
    if (bValidAim && !(SearchingPlayer && SearchingPlayer->Search && SearchingPlayer->Search->bSearching))
        EquippedWeapon->AimAt(Aim, Character->GetActorForwardVector());
    if (!IsCombatInputAllowed()) ReleaseFireIntent();
    else if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        if (!PC->IsInputKeyDown(EKeys::LeftMouseButton)) bFireNeedsRelease = false;
    }
    const bool bRequest = FireIntent.Consume(Config->FireMode);
    if (bRequest && WeaponStates[EquippedIndex].CanFire(*Config, Now) && bValidAim && AimFrame == GFrameCounter && LastFireFrame != GFrameCounter)
        FireShot(*Character, Now);
    UpdateWeaponAudio(Now);
    // FireShot can refresh the assisted center or release it at the actual muzzle.
    bValidAim = Owner->GetCombatAim(Aim, AimFrame) && !Aim.ContainsNaN();
    if (const auto* Player = Cast<ACandyFightCharacter>(Character); Player && Player->CrosshairWidget)
        Player->CrosshairWidget->UpdateWeaponSpread(EquippedWeapon->Muzzle->GetComponentLocation(), Aim,
            GetSpread(), bValidAim && AimFrame == GFrameCounter, Now, GFrameCounter);
}

void UCandyFightCombatComponent::TracePellet(UWorld* World, const FVector& Start, const FVector& Direction, float Range,
    const FCollisionQueryParams& Params, FHitResult& Hit, FVector& End)
{
    End = Start + Direction * Range;
    World->LineTraceSingleByChannel(Hit, Start, End, CandyFightWeapons::TraceChannel, Params);
    if (Hit.bBlockingHit) End = Hit.ImpactPoint;
}

void UCandyFightCombatComponent::FireShot(APawn& Character, double Now)
{
    UCandyFightWeaponConfig& Config = *GetCurrentConfig();
    FCandyFightWeaponState& State = WeaponStates[EquippedIndex];
    FVector Aim; uint64 AimFrame;
    const auto* Owner = Cast<ICandyFightCombatOwner>(&Character);
    if (!Owner || !Owner->IsCombatShotAllowed()) return;
    if (auto* Player = Cast<ACandyFightCharacter>(&Character)) Player->RevalidateCombatAim();
    if (!Owner->GetCombatAim(Aim, AimFrame) || Aim.ContainsNaN()) return;
    if (auto* Player = Cast<ACandyFightCharacter>(&Character))
    {
        EquippedWeapon->AimAt(Aim, Character.GetActorForwardVector());
        Player->RevalidateCombatAim();
        if (!Owner->GetCombatAim(Aim, AimFrame) || Aim.ContainsNaN()) return;
        EquippedWeapon->AimAt(Aim, Character.GetActorForwardVector());
    }
    const FVector Start = EquippedWeapon->Muzzle->GetComponentLocation();
    FVector Direction = Aim - Start;
    Direction.Z = 0.;
    if (Direction.SizeSquared2D() < 25. || FVector::DotProduct(Direction, Character.GetActorForwardVector()) <= 0.)
        Direction = Character.GetActorForwardVector();
    Direction = Direction.GetSafeNormal2D();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightWeapon), false, &Character);
    Params.AddIgnoredActor(EquippedWeapon);
    FVector Origin = Character.GetActorLocation();
    Origin.Z = Start.Z;
    FHitResult Obstruction;
    const bool bObstructed = GetWorld()->SweepSingleByChannel(Obstruction, Origin, Start, FQuat::Identity,
        CandyFightWeapons::TraceChannel, FCollisionShape::MakeSphere(1.f), Params);
    const bool bMuzzleInside = GetWorld()->OverlapBlockingTestByChannel(Start, FQuat::Identity,
        CandyFightWeapons::TraceChannel, FCollisionShape::MakeSphere(1.f), Params);
    TArray<float> Angles;
    CandyFightWeapons::SampleAngles(Config, State.Spread, Random, Angles);
    TArray<FVector> Directions;
    LastShotStart = bObstructed || bMuzzleInside ? Origin : Start;
    LastShotEnds.Reset();
    for (float Angle : Angles)
    {
        const FVector PelletDirection = Direction.RotateAngleAxis(Angle, FVector::UpVector);
        Directions.Add(PelletDirection);
        const FVector End = bObstructed ? Obstruction.ImpactPoint : bMuzzleInside ? Origin : Start + PelletDirection * Config.Range;
        LastShotEnds.Add(End);
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = &Character;
    SpawnParams.Instigator = &Character;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Shot = GetWorld()->SpawnActor<ACandyFightProjectileShot>(LastShotStart, FRotator::ZeroRotator, SpawnParams);
    if (!Shot) return; // No committed shot if its flight instance could not be created.
    Shot->Initialize(Config, Start, Directions, &Character, Character.GetController(), this, EquippedWeapon, Now);
    ActiveShots.RemoveAll([](const auto& Entry) { return !Entry.IsValid(); });
    ActiveShots.Add(Shot);
    CancelActionAudio();
    State.CommitShot(Config, Now, !IsExternallyDriven() && FireIntent.bHeld);
    if (auto* Player = Cast<ACandyFightCharacter>(&Character)) Player->FireCameraFeedback->NotifyShot(Config, Now, Direction);
    PlayFireAudio();
    if (State.bReloading)
    {
        FireIntent.Reset();
        bFireNeedsRelease = true;
    }
    LastFireFrame = GFrameCounter;
    ++TotalShotsFired;
    OnAmmoChanged.Broadcast();
    SyncStateEvents();
    OnShot.Broadcast(EquippedIndex + 1, Angles.Num());
    if (!IsExternallyDriven()) UCandyFightEnemyPerceptionComponent::ReportPlayerShot(&Character, Start);
    if (bObstructed || bMuzzleInside) Shot->ResolveMuzzleObstruction(bObstructed ? Obstruction : FHitResult());
}

void UCandyFightCombatComponent::ClearProjectiles()
{
    for (const auto& Entry : ActiveShots) if (Entry.IsValid()) Entry->Destroy();
    ActiveShots.Reset();
}

void UCandyFightCombatComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ShutdownCombat();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UCandyFightCombatComponent::ClearExternalFireRequest()
{
    if (!IsExternallyDriven()) return;
    StopFire();
    FireIntent.Reset();
}

void UCandyFightCombatComponent::ShutdownCombat()
{
    if (bCombatShutdown) return;
    if (auto* Player = Cast<ACandyFightCharacter>(GetOwner())) Player->ResetAimAssist();
    bCombatShutdown = true;
    // 销毁不等于玩家松开扳机：不能触发空枪自动装填、事件或读取世界时间。
    FireIntent.Reset();
    bFireNeedsRelease = true;
    SetComponentTickEnabled(false);
    ClearProjectiles();
    if (ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
    ActivationHandle.Reset();
    RemoveLocalPlayer();
    for (auto& State : WeaponStates) State.CancelReload();
    if (IsValid(EquippedWeapon)) EquippedWeapon->Destroy();
    EquippedWeapon = nullptr;
}

void UCandyFightCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ShutdownCombat();
    Super::EndPlay(Reason);
}
