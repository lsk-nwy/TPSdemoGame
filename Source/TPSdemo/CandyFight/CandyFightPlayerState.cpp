#include "CandyFightPlayerState.h"
void ACandyFightPlayerState::BeginPlay()
{
    Gold = FMath::Max(0, InitialGold);
    bHasEscaped=false;
    Super::BeginPlay();
}
bool ACandyFightPlayerState::TryMarkEscaped()
{
    if (bHasEscaped || IsActorBeingDestroyed()) return false;
    bHasEscaped=true; return true;
}
int32 ACandyFightPlayerState::AddGold(int32 Amount)
{
    if (Amount <= 0 || IsActorBeingDestroyed()) return 0;
    const int32 Applied = FMath::Min(Amount, MAX_int32 - Gold);
    if (Applied > 0) { Gold += Applied; OnGoldChanged.Broadcast(Gold); }
    return Applied;
}
