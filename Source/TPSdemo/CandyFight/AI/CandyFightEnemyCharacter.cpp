#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyPerceptionComponent.h"
#include "CandyFightEnemyAIController.h"
#include "CandyFightEnemyConfig.h"
#include "CandyFightEnemyWeaponComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/Weapons/CandyFightProjectileShot.h"
#include "CandyFight/Elements/CandyFightElementAuraComponent.h"
#include "CandyFight/Elements/CandyFightElementReactionComponent.h"
#include "CandyFight/Elements/CandyFightFrozenEffectComponent.h"
#include "CandyFight/UI/CandyFightEnemyHealthWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ACandyFightEnemyCharacter::ACandyFightEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
    HealthWidget->SetupAttachment(GetCapsuleComponent());
    HealthWidget->SetRelativeLocation(FVector(0., 0., 118.));
    HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
    HealthWidget->SetDrawSize(FVector2D(140., 40.));
    HealthWidget->SetPivot(FVector2D(.5, 1.));
    HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthWidget->SetGenerateOverlapEvents(false);
    HealthWidget->SetCanEverAffectNavigation(false);
    HealthWidget->SetWidgetClass(UCandyFightEnemyHealthWidget::StaticClass());
    Perception = CreateDefaultSubobject<UCandyFightEnemyPerceptionComponent>(TEXT("Perception"));
    WeaponHoldPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponHoldPoint"));
    WeaponHoldPoint->SetupAttachment(GetCapsuleComponent());
    WeaponHoldPoint->SetRelativeLocation(FVector(30, 48, 20));
    Combat = CreateDefaultSubobject<UCandyFightCombatComponent>(TEXT("Combat"));
    WeaponDriver = CreateDefaultSubobject<UCandyFightEnemyWeaponComponent>(TEXT("WeaponDriver"));
    ElementAura = CreateDefaultSubobject<UCandyFightElementAuraComponent>(TEXT("ElementAura"));
    ElementReactions = CreateDefaultSubobject<UCandyFightElementReactionComponent>(TEXT("ElementReactions"));
    ElementFrozen = CreateDefaultSubobject<UCandyFightFrozenEffectComponent>(TEXT("ElementFrozen"));
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetMesh()->SetCanEverAffectNavigation(false);
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);
	GetCharacterMovement()->MaxWalkSpeed = 200;
	AIControllerClass = ACandyFightEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetRelativeScale3D(FVector(0.6, 0.6, 1.6));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	FacingMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacingMarker"));
	FacingMarker->SetupAttachment(GetCapsuleComponent());
	FacingMarker->SetRelativeLocation(FVector(36, 0, 40));
	FacingMarker->SetRelativeScale3D(FVector(0.35, 0.18, 0.18));
	FacingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FacingMarker->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	Body->SetStaticMesh(Cylinder.Object);
	FacingMarker->SetStaticMesh(Cube.Object);
}

const UCandyFightEnemyConfig& ACandyFightEnemyCharacter::GetPatrolConfig() const
{
	return EnemyConfig && EnemyConfig->IsValidConfiguration() ? *EnemyConfig : *GetDefault<UCandyFightEnemyConfig>();
}

void ACandyFightEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    const float ConfiguredHealth = EnemyConfig ? EnemyConfig->MaxHealth : 100.f;
    MaxHealth = FMath::IsFinite(ConfiguredHealth) && ConfiguredHealth > 0.f ? ConfiguredHealth : 100.f;
    Health = MaxHealth;
    HealthWidget->InitWidget();
    UpdateHealthWidget();
    UpdateHealthVisibility();
    FacingMarker->SetVisibility(!IsValid(Combat->EquippedWeapon));
	GetCharacterMovement()->MaxWalkSpeed = GetPatrolConfig().WalkSpeed;
}

USceneComponent* ACandyFightEnemyCharacter::GetCombatMount() const { return WeaponHoldPoint; }
bool ACandyFightEnemyCharacter::GetCombatAim(FVector& Location, uint64& Frame) const
{
    Location = WeaponDriver->AimLocation; Frame = WeaponDriver->AimFrame;
    return WeaponDriver->bValidAim;
}
UCandyFightWeaponConfig* ACandyFightEnemyCharacter::GetInitialCombatWeapon() const
{
    return WeaponOverride ? WeaponOverride.Get() : EnemyConfig ? EnemyConfig->DefaultWeapon.Get() : nullptr;
}
USoundAttenuation* ACandyFightEnemyCharacter::GetCombatAttenuation() const
{
    return EnemyConfig ? EnemyConfig->WeaponAttenuation.Get() : nullptr;
}

bool ACandyFightEnemyCharacter::IsCombatShotAllowed() const
{
    if (bDefeated) return false;
    // 冻结期间不能开枪；AI 的持续开火请求会被战斗组件丢弃。
    if (ElementFrozen && ElementFrozen->IsFrozen()) return false;
    if (WeaponDriver->IsTesting()) return true;
    const auto* AI = Cast<ACandyFightEnemyAIController>(GetController());
    return AI && AI->CanAutonomousFire();
}

float ACandyFightEnemyCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* InstigatorController, AActor* DamageCauser)
{
    if (bDefeated || IsActorBeingDestroyed() || !CanBeDamaged() ||
        !IsValid(InstigatorController) || !InstigatorController->IsPlayerController() ||
        !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f) return 0.f;

    // 受击先进入反应结算层：附着写入、查表、消耗 GU、乘区/冻结都在这里完成。
    float EffectiveDamage = DamageAmount;
    if (ElementReactions)
    {
        const FCandyFightProjectileDamageEvent* ProjectileEvent =
            DamageEvent.IsOfType(FCandyFightProjectileDamageEvent::ClassID)
            ? static_cast<const FCandyFightProjectileDamageEvent*>(&DamageEvent) : nullptr;
        const FCandyFightReactionHit Hit = ElementReactions->SettleHit(
            ProjectileEvent ? ProjectileEvent->Element : ECandyFightElement::None,
            ProjectileEvent ? ProjectileEvent->ElementGauge : 1.f, DamageAmount);
        EffectiveDamage = Hit.FinalDamage;
    }

    const float Applied = FMath::Min(EffectiveDamage, Health);
    if (Applied <= 0.f) return 0.f;
    Health -= Applied;
    const bool bLethal = Health <= 0.f;
    // Set before any Blueprint notifications: reentrant/sibling hits cannot defeat twice.
    if (bLethal) bDefeated = true;
    UpdateHealthWidget();
    OnHealthChanged.Broadcast(Health, MaxHealth);
    if (!IsActorBeingDestroyed()) Super::TakeDamage(Applied, DamageEvent, InstigatorController, DamageCauser);
    if (bLethal && !IsActorBeingDestroyed()) Defeat(InstigatorController);
    return Applied;
}

void ACandyFightEnemyCharacter::Defeat(AController* InstigatorController)
{
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    SetActorTickEnabled(false);
    HealthWidget->SetVisibility(false);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    Perception->Deactivate();
    WeaponDriver->StopTest(false);
    WeaponDriver->SetComponentTickEnabled(false);
    Combat->ShutdownCombat();
    // OnUnPossess stops both behavior trees and path following without resuming patrol.
    if (AController* AI = GetController())
    {
        AI->UnPossess();
        AI->Destroy();
    }
    OnEnemyDefeated.Broadcast(this, InstigatorController);
    Destroy();
}

void ACandyFightEnemyCharacter::UpdateHealthWidget()
{
    if (auto* Widget = Cast<UCandyFightEnemyHealthWidget>(HealthWidget->GetWidget()))
        Widget->SetHealthDisplay(Health, MaxHealth);
}

bool ACandyFightEnemyCharacter::IsHealthBarUnoccluded(const FVector& CameraLocation, const AActor* Viewer) const
{
    if (!GetWorld() || bDefeated) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightHealthVisibility), false, this);
    if (Viewer) Params.AddIgnoredActor(Viewer);
    if (Combat->EquippedWeapon) Params.AddIgnoredActor(Combat->EquippedWeapon);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, HealthWidget->GetComponentLocation(), ECC_Visibility, Params);
}

void ACandyFightEnemyCharacter::UpdateHealthVisibility()
{
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    bool bVisible = false;
    if (!bDefeated && PC && PC->PlayerCameraManager)
    {
        FVector2D Screen;
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        bVisible = PC->ProjectWorldLocationToScreen(HealthWidget->GetComponentLocation(), Screen) &&
            Screen.X >= 0 && Screen.Y >= 0 && Screen.X < Width && Screen.Y < Height &&
            IsHealthBarUnoccluded(PC->PlayerCameraManager->GetCameraLocation(), PC->GetPawn());
    }
    HealthWidget->SetVisibility(bVisible);
}

void ACandyFightEnemyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateHealthVisibility();
}
