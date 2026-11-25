#include "CandyFight/Elements/CandyFightFrozenEffectComponent.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectGlobals.h"

UCandyFightFrozenEffectComponent::UCandyFightFrozenEffectComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UCandyFightFrozenEffectComponent::Freeze(float Seconds, float Gauge)
{
    UWorld* World = GetWorld();
    if (!World || !FMath::IsFinite(Seconds) || Seconds <= 0.f) return false;
    const double Now = World->GetTimeSeconds();
    if (!bFrozen)
    {
        bFrozen = true;
        FrozenUntil = Now + Seconds;
        BreakDamageAccumulated = 0.f;
        FrozenGauge = FMath::Max(0.f, Gauge);
        ApplyState();
    }
    else
    {
        // 已经冻结：只刷新剩余时间，不重复播放一次性表现。
        FrozenUntil = FMath::Max(FrozenUntil, Now + Seconds);
        FrozenGauge = FMath::Max(FrozenGauge, Gauge);
        UpdateLabel();
    }
    OnFrozenChanged.Broadcast(true, GetRemainingSecondsAt(Now));
    return true;
}

void UCandyFightFrozenEffectComponent::Unfreeze(bool bShatter)
{
    if (!bFrozen) return;
    bFrozen = false;
    FrozenUntil = 0.;
    FrozenGauge = 0.f;
    BreakDamageAccumulated = 0.f;
    RestoreState();
    if (bShatter && ShatterEffect && GetWorld())
    {
        if (AActor* Owner = GetOwner())
            ActiveShatterEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ShatterEffect,
                Owner->GetActorLocation() + FVector(0., 0., 60.));
    }
    UpdateLabel();
    OnFrozenChanged.Broadcast(false, 0.f);
}

bool UCandyFightFrozenEffectComponent::NotifyDamage(float Damage)
{
    if (!bFrozen || !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
    BreakDamageAccumulated += Damage;
    if (BreakDamageThreshold > 0.f && BreakDamageAccumulated >= BreakDamageThreshold)
    {
        Unfreeze(false);
        return true;
    }
    return false;
}

float UCandyFightFrozenEffectComponent::GetRemainingSeconds() const
{
    if (const UWorld* World = GetWorld()) return GetRemainingSecondsAt(World->GetTimeSeconds());
    return 0.f;
}

float UCandyFightFrozenEffectComponent::GetRemainingSecondsAt(double Now) const
{
    if (!bFrozen || !FMath::IsFinite(Now)) return 0.f;
    return float(FMath::Max(0., FrozenUntil - Now));
}

void UCandyFightFrozenEffectComponent::AdvanceTo(double Now)
{
    if (!bFrozen || !FMath::IsFinite(Now)) return;
    if (Now < FrozenUntil)
    {
        UpdateLabel();
        return;
    }
    Unfreeze(false);
}

void UCandyFightFrozenEffectComponent::ApplyState()
{
    StateTags.Reset();
    StateTags.AddTag(CandyFightElements::GetReactionTag(ECandyFightReaction::Freeze));
    StateTags.AddTag(CandyFightTags_State_Frozen.GetTag());

    if (AActor* Owner = GetOwner(); Owner && !Owner->IsActorBeingDestroyed())
    {
        if (ACharacter* Character = Cast<ACharacter>(Owner))
        {
            if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
            {
                SavedMovementMode = static_cast<uint8>(Movement->MovementMode);
                bHasSavedMovementMode = true;
                Movement->StopMovementImmediately();
                Movement->DisableMovement();
            }
            if (AAIController* AI = Cast<AAIController>(Character->GetController())) AI->StopMovement();
        }
        if (UCandyFightCombatComponent* Combat = Owner->FindComponentByClass<UCandyFightCombatComponent>())
        {
            Combat->StopFire();
            Combat->ClearExternalFireRequest();
        }
    }

    TintMaterials();

    if (FreezeEffect && GetWorld())
    {
        if (AActor* Owner = GetOwner())
        {
            if (USceneComponent* Root = Owner->GetRootComponent())
                ActiveFreezeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(FreezeEffect, Root, NAME_None,
                    FVector(0., 0., 60.), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
            else
                ActiveFreezeEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), FreezeEffect,
                    Owner->GetActorLocation() + FVector(0., 0., 60.));
        }
    }
    EnsureLabel();
    UpdateLabel();
}

void UCandyFightFrozenEffectComponent::RestoreState()
{
    RemoveStateTag();
    if (AActor* Owner = GetOwner(); Owner && !Owner->IsActorBeingDestroyed())
    {
        if (ACharacter* Character = Cast<ACharacter>(Owner))
        {
            if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
            {
                const EMovementMode Mode = bHasSavedMovementMode ? static_cast<EMovementMode>(SavedMovementMode) : MOVE_Walking;
                Movement->SetMovementMode(Mode == MOVE_None ? MOVE_Walking : Mode);
            }
        }
    }
    bHasSavedMovementMode = false;
    RestoreMaterials();
    if (UNiagaraComponent* Effect = ActiveFreezeEffect.Get()) Effect->Deactivate();
    ActiveFreezeEffect.Reset();
}

void UCandyFightFrozenEffectComponent::RemoveStateTag()
{
    StateTags.Reset();
}

void UCandyFightFrozenEffectComponent::TintMaterials()
{
    AActor* Owner = GetOwner();
    if (!Owner || Owner->IsActorBeingDestroyed() || !bTintOwnerMaterials) return;
    TInlineComponentArray<UMeshComponent*> Meshes(Owner);
    for (UMeshComponent* Mesh : Meshes)
    {
        if (!Mesh || !Mesh->IsRegistered()) continue;
        const int32 SlotCount = Mesh->GetNumMaterials();
        if (SlotCount <= 0) continue;
        FCandyFightFrozenMaterialBackup Backup;
        Backup.Mesh = Mesh;
        for (int32 Slot = 0; Slot < SlotCount; ++Slot)
        {
            UMaterialInterface* Original = Mesh->GetMaterial(Slot);
            Backup.Materials.Add(Original);
            UMaterialInterface* Parent = FrostMaterial ? FrostMaterial.Get() : Original;
            if (!Parent) continue;
            UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Parent, this);
            if (!Instance) continue;
            Instance->SetVectorParameterValue(TintParameterName, TintColor);
            FrostMIDs.Add(Instance);
            Mesh->SetMaterial(Slot, Instance);
        }
        MaterialBackups.Add(MoveTemp(Backup));
    }
}

void UCandyFightFrozenEffectComponent::RestoreMaterials()
{
    for (FCandyFightFrozenMaterialBackup& Backup : MaterialBackups)
    {
        UMeshComponent* Mesh = Backup.Mesh.Get();
        if (!Mesh) continue;
        const int32 SlotCount = FMath::Min(Backup.Materials.Num(), Mesh->GetNumMaterials());
        for (int32 Slot = 0; Slot < SlotCount; ++Slot) Mesh->SetMaterial(Slot, Backup.Materials[Slot].Get());
    }
    MaterialBackups.Reset();
    FrostMIDs.Reset();
}

void UCandyFightFrozenEffectComponent::EnsureLabel()
{
    if (StateLabel || !bShowStateLabel) return;
    AActor* Owner = GetOwner();
    if (!Owner) return;
    const FName LabelName = MakeUniqueObjectName(Owner, UTextRenderComponent::StaticClass(), TEXT("CandyFightFrozenLabel"));
    StateLabel = NewObject<UTextRenderComponent>(Owner, LabelName);
    if (!StateLabel) return;
    StateLabel->RegisterComponent();
    if (USceneComponent* Root = Owner->GetRootComponent())
        StateLabel->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
    StateLabel->SetRelativeLocation(FVector(0., 0., FMath::Max(1.f, StateLabelHeight)));
    StateLabel->SetHorizontalAlignment(EHTA_Center);
    StateLabel->SetWorldSize(FMath::Max(1.f, StateLabelSize));
    StateLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StateLabel->SetGenerateOverlapEvents(false);
    StateLabel->SetCanEverAffectNavigation(false);
    StateLabel->SetVisibility(false);
}

void UCandyFightFrozenEffectComponent::UpdateLabel()
{
    if (!bShowStateLabel) return;
    if (!bFrozen)
    {
        if (StateLabel) StateLabel->SetVisibility(false);
        StateLabelText.Reset();
        return;
    }
    EnsureLabel();
    const float Remaining = GetRemainingSeconds();
    StateLabelText = FString::Printf(TEXT("FROZEN %.1fs"), Remaining);
    if (StateLabel)
    {
        StateLabel->SetText(FText::FromString(StateLabelText));
        StateLabel->SetTextRenderColor(CandyFightElements::GetElementColor(ECandyFightElement::Ice).ToFColor(true));
        StateLabel->SetVisibility(true);
    }
}

void UCandyFightFrozenEffectComponent::FaceCamera()
{
    if (!StateLabel || !StateLabel->IsVisible()) return;
    if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const FVector ToCamera = Camera->GetCameraLocation() - StateLabel->GetComponentLocation();
        if (!ToCamera.IsNearlyZero()) StateLabel->SetWorldRotation(ToCamera.Rotation());
    }
}

void UCandyFightFrozenEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bFrozen)
    {
        if (const UWorld* World = GetWorld()) AdvanceTo(World->GetTimeSeconds());
        if (bFrozen) UpdateLabel();
        FaceCamera();
    }
}

void UCandyFightFrozenEffectComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bFrozen) Unfreeze(false);
    Super::EndPlay(Reason);
}
