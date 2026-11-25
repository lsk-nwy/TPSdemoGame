#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CandyFightPatrolRoute.generated.h"

/** Points are local to this actor; shared routes never store an enemy's cursor. */
UCLASS()
class TPSDEMO_API ACandyFightPatrolRoute : public AActor
{
	GENERATED_BODY()
public:
	ACandyFightPatrolRoute();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(MakeEditWidget=true))
	TArray<FVector> Points;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol")
	bool bPingPong = false;
	FVector GetWorldPoint(int32 Index) const;
	static void AdvanceCursor(int32 Count, bool bBounce, int32& Index, int32& Direction);
	static bool RegisterFailure(int32 Limit, int32& Failures);
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
};
