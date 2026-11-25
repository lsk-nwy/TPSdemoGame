#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CandyFight/Weapons/CandyFightCombatOwner.h"
#include "CandyFight/Elements/CandyFightElementTypes.h"
#include "CandyFightEnemyCharacter.generated.h"

class UCandyFightEnemyConfig;
class ACandyFightPatrolRoute;
class UBehaviorTree;
class UStaticMeshComponent;
class UWidgetComponent;
class ACandyFightEnemyCharacter;
class UCandyFightElementAuraComponent;
class UCandyFightElementReactionComponent;
class UCandyFightFrozenEffectComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightEnemyHealthChanged, float, Current, float, Max);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightEnemyDefeated, ACandyFightEnemyCharacter*, Enemy, AController*, InstigatorController);

UCLASS()
class TPSDEMO_API ACandyFightEnemyCharacter : public ACharacter, public ICandyFightCombatOwner
{
	GENERATED_BODY()
public:
	ACandyFightEnemyCharacter();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Health") TObjectPtr<UWidgetComponent> HealthWidget;
    UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Health") float Health = 100.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Health") float MaxHealth = 100.f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="CandyFight|Health") bool bDefeated = false;
    UPROPERTY(BlueprintAssignable, Category="CandyFight|Health") FCandyFightEnemyHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable, Category="CandyFight|Health") FCandyFightEnemyDefeated OnEnemyDefeated;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* InstigatorController, AActor* DamageCauser) override;
    virtual void Tick(float DeltaSeconds) override;
    // Uses the same visibility trace as the head bar; also useful to verify occlusion.
    bool IsHealthBarUnoccluded(const FVector& CameraLocation, const AActor* Viewer) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USceneComponent> WeaponHoldPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<class UCandyFightCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<class UCandyFightEnemyWeaponComponent> WeaponDriver;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UCandyFightWeaponConfig> WeaponOverride;
    /** 元素附着层：只记录 GU 与衰减。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Elements") TObjectPtr<UCandyFightElementAuraComponent> ElementAura;
    /** 反应结算层：查表、消耗 GU、输出伤害乘区/冻结。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Elements") TObjectPtr<UCandyFightElementReactionComponent> ElementReactions;
    /** 冻结状态层：禁用移动/射击、染色、受击解冻。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Elements") TObjectPtr<UCandyFightFrozenEffectComponent> ElementFrozen;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Weapon|Test") TObjectPtr<AActor> TestTarget;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Weapon|Test", meta=(ClampMin="1")) int32 TestId = 0;
    UPROPERTY(EditInstanceOnly, AdvancedDisplay, Category="Weapon|Test") int32 WeaponTestMigrationVersion = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Autonomy") TObjectPtr<class UCandyFightEnemyPerceptionComponent> Perception;
    virtual bool IsCombatShotAllowed() const override;
    virtual USceneComponent* GetCombatMount() const override;
    virtual bool GetCombatAim(FVector& Location, uint64& Frame) const override;
    virtual bool IsExternalCombatDriver() const override { return true; }
    virtual UCandyFightWeaponConfig* GetInitialCombatWeapon() const override;
    virtual USoundAttenuation* GetCombatAttenuation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CandyFight|AI")
	TObjectPtr<UCandyFightEnemyConfig> EnemyConfig;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CandyFight|AI")
	TObjectPtr<UBehaviorTree> PatrolBehaviorTree;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="CandyFight|AI")
	TObjectPtr<ACandyFightPatrolRoute> PatrolRoute;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Appearance")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CandyFight|Appearance")
	TObjectPtr<UStaticMeshComponent> FacingMarker;
	const UCandyFightEnemyConfig& GetPatrolConfig() const;
	virtual void BeginPlay() override;
private:
    void UpdateHealthWidget();
    void UpdateHealthVisibility();
    void Defeat(AController* InstigatorController);
};
