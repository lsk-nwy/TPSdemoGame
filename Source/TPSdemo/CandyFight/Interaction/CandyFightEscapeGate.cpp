#include "CandyFightEscapeGate.h"
#include "CandyFight/Flow/CandyFightFlow.h"
#include "CandyFightEscapeComponent.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/UI/CandyFightEscapeGateWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "TPSdemo.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ACandyFightEscapeGate::ACandyFightEscapeGate()
{
    PrimaryActorTick.bCanEverTick=true; SetCanBeDamaged(false);
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
    Disc=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc")); Disc->SetupAttachment(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    Disc->SetStaticMesh(Cylinder.Object); Disc->SetRelativeLocation(FVector(0,0,3)); Disc->SetRelativeScale3D(FVector(3,3,.06));
    Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Disc->SetCanEverAffectNavigation(false); Disc->SetGenerateOverlapEvents(false);
    RangePreview=CreateDefaultSubobject<USphereComponent>(TEXT("RangePreview")); RangePreview->SetupAttachment(Root);
    RangePreview->SetSphereRadius(150); RangePreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RangePreview->SetCanEverAffectNavigation(false); RangePreview->SetGenerateOverlapEvents(false); RangePreview->SetHiddenInGame(true);
    RangePreview->ShapeColor=FColor::Turquoise; RangePreview->SetLineThickness(2.f);
    InteractionPoint=CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint")); InteractionPoint->SetupAttachment(Root);
    InteractionPoint->SetRelativeLocation(FVector(0,0,50));
    GateWidget=CreateDefaultSubobject<UWidgetComponent>(TEXT("GateWidget")); GateWidget->SetupAttachment(Root);
    GateWidget->SetRelativeLocation(FVector(0,0,100)); GateWidget->SetWidgetSpace(EWidgetSpace::Screen);
    GateWidget->SetDrawSize(FVector2D(180,96)); GateWidget->SetPivot(FVector2D(.5,1));
    GateWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); GateWidget->SetCanEverAffectNavigation(false);
    GateWidget->SetWidgetClass(UCandyFightEscapeGateWidget::StaticClass()); GateWidget->SetVisibility(false);
}
bool ACandyFightEscapeGate::ValidateConfiguration(FString& Error) const
{
    if (!FMath::IsFinite(OpenSeconds) || OpenSeconds<=0 || !FMath::IsFinite(EscapeSeconds) || EscapeSeconds<=0 ||
        !FMath::IsFinite(EscapeRadius) || EscapeRadius<=0 || !FMath::IsFinite(MaxHeightDifference) || MaxHeightDifference<0)
    { Error=TEXT("Escape gate durations/radius must be finite and positive; height must be finite and nonnegative."); return false; }
    return true;
}
void ACandyFightEscapeGate::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (RangePreview) RangePreview->SetSphereRadius(FMath::IsFinite(EscapeRadius) ? FMath::Max(1.f,EscapeRadius) : 150.f);
}
void ACandyFightEscapeGate::BeginPlay()
{
    Super::BeginPlay(); bStarted=true;
    RuntimeEscapeSeconds=EscapeSeconds; RuntimeRadius=EscapeRadius; RuntimeHeight=MaxHeightDifference;
    GateWidget->InitWidget();
    if (Disc->GetMaterial(0)) DiscMaterial=Disc->CreateDynamicMaterialInstance(0);
    FString Error;
    if (ValidateConfiguration(Error)) { CloseTime=GetWorld()->GetTimeSeconds()+double(OpenSeconds); State=ECandyFightEscapeGateState::Open; }
    else { UE_LOG(LogTPSdemo,Warning,TEXT("Escape gate %s disabled: %s"),*GetName(),*Error); CloseGate(); }
    UpdatePresentation();
}
float ACandyFightEscapeGate::GetRemainingCloseSeconds() const
{
    return bStarted && State==ECandyFightEscapeGateState::Open && GetWorld() ? float(FMath::Max(0.,CloseTime-GetWorld()->GetTimeSeconds())) : 0.f;
}
bool ACandyFightEscapeGate::IsOpen() const { return !IsActorBeingDestroyed() && GetRemainingCloseSeconds()>0.f; }
bool ACandyFightEscapeGate::CanEscape(const ACandyFightCharacter* Player) const
{
    if (!IsOpen() || !IsValid(Player) || Player->IsActorBeingDestroyed() || !Player->GetController()) return false;
    const FVector Position=Player->GetActorLocation();
    const double Feet=Position.Z-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (FVector::DistSquared2D(Position,GetActorLocation())>FMath::Square(RuntimeRadius) || FMath::Abs(Feet-GetActorLocation().Z)>RuntimeHeight) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightEscapeReach),false,Player);
    Params.AddIgnoredActor(this);
    if (Player->Combat->EquippedWeapon) Params.AddIgnoredActor(Player->Combat->EquippedWeapon.Get());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Position,InteractionPoint->GetComponentLocation(),ECC_Visibility,Params);
}
void ACandyFightEscapeGate::CloseGate()
{
    State=ECandyFightEscapeGateState::Closed; SuccessUntil=0.;
    if (DiscMaterial) DiscMaterial->SetScalarParameterValue(TEXT("GateOpen"),0.f);
    if (GetWorld()) for (TActorIterator<ACandyFightCharacter> It(GetWorld());It;++It)
        if (It->Escape && It->Escape->TargetGate==this) It->Escape->CancelEscape();
    if (!bClosedNotified) { bClosedNotified=true; OnClosed.Broadcast(); }
}
bool ACandyFightEscapeGate::CompleteEscape(ACandyFightCharacter* Player)
{
    if (!IsOpen()) { CloseGate(); return false; }
    auto* Wallet=IsValid(Player) ? Player->GetPlayerState<ACandyFightPlayerState>() : nullptr;
    if (!Wallet || !CanEscape(Player) || !Player->Escape || !Player->Escape->bEscaping ||
        Player->Escape->TargetGate!=this || Player->Escape->GetRemainingEscapeSeconds()>0.f || !Wallet->TryMarkEscaped()) return false;
    float Seconds=2.f;
    if (const auto* Widget=Cast<UCandyFightEscapeGateWidget>(GateWidget->GetUserWidgetObject())) Seconds=Widget->SuccessSeconds;
    SuccessUntil=GetWorld()->GetTimeSeconds()+(FMath::IsFinite(Seconds) ? FMath::Max(0.f,Seconds) : 0.f);
    Player->Escape->CancelEscape();
    OnPlayerEscaped.Broadcast(Player);
    if (auto* Flow = Cast<UCandyFightGameInstance>(GetGameInstance())) Flow->FinishBattle(ECandyFightRunOutcome::Escaped);
    return true;
}
void ACandyFightEscapeGate::UpdatePresentation()
{
    if (DiscMaterial) DiscMaterial->SetScalarParameterValue(TEXT("GateOpen"),IsOpen() ? 1.f : 0.f);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* Player=PC ? Cast<ACandyFightCharacter>(PC->GetPawn()) : nullptr;
    FVector2D Screen; int32 Width=0,Height=0;
    if (PC) PC->GetViewportSize(Width,Height);
    bool Visible=PC && PC->IsLocalController() && Player && PC->ProjectWorldLocationToScreen(GateWidget->GetComponentLocation(),Screen,true) &&
        Screen.X>=0 && Screen.Y>=0 && Screen.X<=Width && Screen.Y<=Height;
    if (Visible)
    {
        FVector Camera; FRotator Rotation; PC->GetPlayerViewPoint(Camera,Rotation);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightEscapePrompt),false,Player); Params.AddIgnoredActor(this);
        if (Player->Combat->EquippedWeapon) Params.AddIgnoredActor(Player->Combat->EquippedWeapon.Get());
        FHitResult Hit; Visible=!GetWorld()->LineTraceSingleByChannel(Hit,Camera,GateWidget->GetComponentLocation(),ECC_Visibility,Params);
    }
    GateWidget->SetVisibility(Visible);
    if (Visible) if (auto* Widget=Cast<UCandyFightEscapeGateWidget>(GateWidget->GetUserWidgetObject()))
    {
        const auto* Escape=Player->Escape.Get(); const bool Waiting=Escape && Escape->bEscaping && Escape->TargetGate==this;
        Widget->SetDisplay(IsOpen(),GetRemainingCloseSeconds(),Waiting,Waiting ? Escape->GetEscapeProgress() : 0.f,
            Waiting ? Escape->GetRemainingEscapeSeconds() : 0.f,IsOpen() && GetWorld()->GetTimeSeconds()<SuccessUntil);
    }
}
void ACandyFightEscapeGate::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (State==ECandyFightEscapeGateState::Open && !IsOpen()) CloseGate();
    UpdatePresentation();
}
void ACandyFightEscapeGate::EndPlay(const EEndPlayReason::Type Reason)
{
    State=ECandyFightEscapeGateState::Closed;
    if (GetWorld()) for (TActorIterator<ACandyFightCharacter> It(GetWorld());It;++It)
        if (It->Escape && It->Escape->TargetGate==this) It->Escape->CancelEscape();
    Super::EndPlay(Reason);
}
#if WITH_EDITOR
EDataValidationResult ACandyFightEscapeGate::IsDataValid(FDataValidationContext& Context) const
{
    const auto Parent=Super::IsDataValid(Context); FString Error;
    if (!ValidateConfiguration(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
    return Parent==EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
