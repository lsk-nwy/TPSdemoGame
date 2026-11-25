#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CandyFightCombatOwner.generated.h"
class USceneComponent;
class UCandyFightWeaponConfig;
class USoundAttenuation;

// 玩家和 AI 只提供瞄准与挂点；发射规则继续由同一个战斗组件执行。
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UCandyFightCombatOwner : public UInterface { GENERATED_BODY() };
class TPSDEMO_API ICandyFightCombatOwner
{
    GENERATED_BODY()
public:
    virtual USceneComponent* GetCombatMount() const = 0;
    virtual bool GetCombatAim(FVector& Location, uint64& Frame) const = 0;
    virtual bool IsCombatShotAllowed() const { return true; }
    virtual bool IsExternalCombatDriver() const { return false; }
    virtual UCandyFightWeaponConfig* GetInitialCombatWeapon() const { return nullptr; }
    virtual USoundAttenuation* GetCombatAttenuation() const { return nullptr; }
};
