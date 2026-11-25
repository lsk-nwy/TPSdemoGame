#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CandyFightWeapon.generated.h"

class UStaticMeshComponent;

/** +X is barrel forward. Blueprint component edits are never rebuilt by gameplay code. */
UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightWeapon : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightWeapon();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USceneComponent> MountRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USceneComponent> WeaponVisualRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> Barrel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> Grip;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USceneComponent> Muzzle;
    /** Optional per-weapon adjustment relative to the character's WeaponHoldPoint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") FTransform MountTransform = FTransform::Identity;
    UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category="Weapon") int32 HoldPointMigrationVersion = 0;

    bool ValidateMuzzle() const;
    void AimAt(const FVector& Target, const FVector& FallbackForward);
protected:
    virtual void BeginPlay() override;
};
