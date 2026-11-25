#include "CandyFightPatrolRoute.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ACandyFightPatrolRoute::ACandyFightPatrolRoute()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot"));
	// Route drawing is editor-only; packaged routes are data, not ticking actors.
	PrimaryActorTick.bCanEverTick = WITH_EDITOR;
	PrimaryActorTick.TickInterval = 0.1f;
	Points = {FVector::ZeroVector, FVector(400, 0, 0), FVector(400, 400, 0)};
}

FVector ACandyFightPatrolRoute::GetWorldPoint(int32 Index) const
{
	return GetActorTransform().TransformPosition(Points.IsValidIndex(Index) ? Points[Index] : FVector::ZeroVector);
}

void ACandyFightPatrolRoute::AdvanceCursor(int32 Count, bool bBounce, int32& Index, int32& Direction)
{
	if (Count <= 1) { Index = 0; Direction = 1; return; }
	Index = FMath::Clamp(Index, 0, Count - 1);
	Direction = Direction < 0 ? -1 : 1;
	if (!bBounce) { Index = (Index + 1) % Count; Direction = 1; return; }
	if (Index == Count - 1) Direction = -1;
	else if (Index == 0) Direction = 1;
	Index += Direction;
}

bool ACandyFightPatrolRoute::RegisterFailure(int32 Limit, int32& Failures)
{
	if (++Failures < FMath::Max(1, Limit)) return false;
	Failures = 0;
	return true;
}

void ACandyFightPatrolRoute::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if WITH_EDITOR
	if (GetWorld()->IsGameWorld()) { SetActorTickEnabled(false); return; }
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		const FVector P = GetWorldPoint(I);
		DrawDebugSphere(GetWorld(), P, 24, 8, FColor::Cyan, false, 0.12f);
		DrawDebugString(GetWorld(), P + FVector(0, 0, 35), FString::FromInt(I), nullptr, FColor::White, 0.12f);
		if (I > 0) DrawDebugLine(GetWorld(), GetWorldPoint(I - 1), P, FColor::Cyan, false, 0.12f, 0, 2);
	}
	if (!bPingPong && Points.Num() > 2)
		DrawDebugLine(GetWorld(), GetWorldPoint(Points.Num() - 1), GetWorldPoint(0), FColor::Cyan, false, 0.12f, 0, 2);
#endif
}
