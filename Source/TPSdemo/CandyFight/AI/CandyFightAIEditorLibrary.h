#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CandyFightAIEditorLibrary.generated.h"

/** Editor-only authoring helpers called by Scripts/CandyFight/AI/setup_patrol.py. */
UCLASS()
class TPSDEMO_API UCandyFightAIEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
    static bool CreateAutonomyTree();
    UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
    static bool VerifyCombatNavigation(UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
	static bool CreatePatrolTree();
	/** Compile the delivered enemy and inspect compiler errors (Status is protected in Python). */
	UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
	static bool CompileEnemyBlueprint();
	UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
	static bool BuildTestNavigation(UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
	static AActor* BeginTestActorSpawn(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FVector Location);
	UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
	static AActor* FinishTestActorSpawn(AActor* Actor);
	UFUNCTION(BlueprintCallable, Category="CandyFight|AI|Editor")
	static bool CaptureActiveEditorWindow(const FString& Filename);
};
