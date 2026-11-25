#pragma once
#include "CoreMinimal.h"
#include "NavigationPath.h"
class ACandyFightEnemyCharacter;
/** 纯查询模块：不发起移动，也不修改枪模；控制器独占导航请求。 */
namespace CandyFightPositioning
{
    bool InsideLeash(const FVector& Point, const FVector& Anchor, float Radius);
    // 射击站位需要比普通调查/归位更精确；同时适配较窄的交战距离区间。
    float PositionAcceptanceRadius(const ACandyFightEnemyCharacter& Enemy, float MinDistance, float MaxDistance);
    bool FindPath(ACandyFightEnemyCharacter& Enemy, const FVector& Candidate, const FVector& Anchor, float Radius,
        FVector& Projected, FNavPathSharedPtr& Path, bool bConstrainLeash = true, FString* FailureReason = nullptr);
    // 枪声/视线线索在空中。先独立处理高度，再复用严格的地面路径与追踪范围检查。
    bool FindCluePath(ACandyFightEnemyCharacter& Enemy, const FVector& Clue, const FVector& Anchor, float Radius,
        FVector& Projected, FNavPathSharedPtr& Path, FString* FailureReason = nullptr);
    bool HasFiringLine(const ACandyFightEnemyCharacter& Enemy, const AActor& Target, const FVector& Start, const FVector& OwnerCenter, float Range);
    bool CandidateMuzzle(const ACandyFightEnemyCharacter& Enemy, const FVector& Feet, const FVector& Target, FVector& Muzzle, FVector& Center);
    bool FindPosition(ACandyFightEnemyCharacter& Enemy, const AActor& Target, const FVector& Anchor,
        float MinDistance, float MaxDistance, const FVector* Previous, FVector& Destination, FNavPathSharedPtr& Path);
}
