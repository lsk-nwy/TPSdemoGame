#include "CandyFightEnemyCharacter.h"
#include "CandyFightEnemyWeaponComponent.h"
#include "CandyFightEnemyAIController.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
namespace
{
void Execute(const TArray<FString>& Args, UWorld* World, FOutputDevice& Out, int32 Action)
{
    if (!World || !World->IsGameWorld()) { Out.Log(TEXT("Run this command in Play / a game world.")); return; }
    if (Action == 0 || Action == 3)
    {
        for (TActorIterator<ACandyFightEnemyCharacter> It(World); It; ++It)
        {
            auto* Enemy = *It;
            if (Action == 3) Enemy->WeaponDriver->StopTest();
            else Out.Logf(TEXT("ID=%d %s weapon=%s target=%s ammo=%d testing=%d groupShots=%d completedGroups=%d reload=%d"),
                Enemy->TestId, *Enemy->GetName(), *GetNameSafe(Enemy->Combat->GetCurrentConfig()), *GetNameSafe(Enemy->TestTarget),
                Enemy->Combat->GetAmmo(), Enemy->WeaponDriver->IsTesting(), Enemy->WeaponDriver->Burst.ShotsInGroup,
                Enemy->WeaponDriver->Burst.CompletedGroups, int32(Enemy->Combat->GetReloadPhase()));
            if (Action == 0)
                if (auto* AI=Cast<ACandyFightEnemyAIController>(Enemy->GetController()))
                    Out.Logf(TEXT("  state=%s reason=%s"), *StaticEnum<ECandyFightEnemyState>()->GetNameStringByValue((int64)AI->CombatState), *AI->StopReason);
        }
        if (Action == 3) Out.Log(TEXT("All enemy weapon tests stopped."));
        return;
    }
    int32 Id = 0;
    if (Args.Num() != 1 || !LexTryParseString(Id, *Args[0]) || Id <= 0)
        { Out.Log(TEXT("Usage: EnemyWeaponTestStart <positive ID> / EnemyWeaponTestStop <positive ID>")); return; }
    ACandyFightEnemyCharacter* Match = nullptr;
    for (TActorIterator<ACandyFightEnemyCharacter> It(World); It; ++It)
        if (It->TestId == Id)
        {
            if (Match) { Out.Log(TEXT("Duplicate TestId; no state changed.")); return; }
            Match = *It;
        }
    if (!Match) { Out.Log(TEXT("TestId not found; use EnemyWeaponTestList.")); return; }
    FString Error;
    if (Action == 1 && !Match->WeaponDriver->StartTest(Error)) { Out.Log(*Error); return; }
    if (Action == 2) Match->WeaponDriver->StopTest();
    Out.Logf(TEXT("Enemy %d weapon test %s"), Id, Action == 1 ? TEXT("started") : TEXT("stopped"));
}
FAutoConsoleCommandWithWorldArgsAndOutputDevice EnemyWeaponListCommand(TEXT("EnemyWeaponTestList"), TEXT("List enemy weapons and test IDs"),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W,FOutputDevice& O){Execute(A,W,O,0);}));
FAutoConsoleCommandWithWorldArgsAndOutputDevice EnemyWeaponStartCommand(TEXT("EnemyWeaponTestStart"), TEXT("Start grouped fire at the assigned target: <ID>"),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W,FOutputDevice& O){Execute(A,W,O,1);}));
FAutoConsoleCommandWithWorldArgsAndOutputDevice EnemyWeaponStopCommand(TEXT("EnemyWeaponTestStop"), TEXT("Stop firing and resume patrol: <ID>"),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W,FOutputDevice& O){Execute(A,W,O,2);}));
FAutoConsoleCommandWithWorldArgsAndOutputDevice EnemyWeaponStopAllCommand(TEXT("EnemyWeaponTestStopAll"), TEXT("Stop all enemy weapon tests"),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W,FOutputDevice& O){Execute(A,W,O,3);}));
}
#endif
