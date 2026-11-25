#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "CandyFightChest.generated.h"

class ACandyFightCharacter;
class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class UWidgetComponent;
class UCandyFightChestInteractionWidget;
class UTextRenderComponent;

UCLASS(BlueprintType)
class TPSDEMO_API UCandyFightChestConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01", Units="s")) float SearchSeconds = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", Units="cm")) float InteractionDistance = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="cm")) float MaxHeightDifference = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 MinGold = 10;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 MaxGold = 100;
    bool Validate(FString& Error) const;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UENUM(BlueprintType)
enum class ECandyFightChestState : uint8 { Unsearched, Searching, Searched };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCandyFightChestLooted, ACandyFightCharacter*, Player, int32, GoldAmount);

UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightChest : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightChest();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<UCandyFightChestConfig> Config;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<USceneComponent> LidHinge;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<UStaticMeshComponent> Lid;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<USceneComponent> InteractionPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chest") TObjectPtr<UWidgetComponent> InteractionWidget;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest", meta=(Units="deg")) float OpenAngle = 110.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest", meta=(ClampMin="0", Units="s")) float OpenSeconds = .35f;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Chest") ECandyFightChestState State = ECandyFightChestState::Unsearched;
    UPROPERTY(BlueprintAssignable, Category="Chest") FCandyFightChestLooted OnLooted;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Chest") int32 GrantedGold = 0;
    // Spawner sets before BeginPlay; no reward reroll when a search is cancelled.
    int32 LootSeed = 0;
    UFUNCTION(BlueprintPure, Category="Chest") const UCandyFightChestConfig* GetConfig() const;
    bool IsInReach(const ACandyFightCharacter* Player) const;
    bool TryClaim(ACandyFightCharacter* Player);
    void ReleaseClaim(ACandyFightCharacter* Player);
    bool CompleteSearch(ACandyFightCharacter* Player);
    void ShowInterrupted();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* Controller, AActor* Causer) override { return 0.f; }
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FCandyFightChestInteractionTest;
    TWeakObjectPtr<ACandyFightCharacter> Claimant;
    int32 Reward = 0;
    FRotator ClosedLidRotation;
    double OpenStart = 0.;
    double FeedbackEnd = 0.;
    bool bInterruptedFeedback = false;
    void UpdatePresentation();
};

UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightChestSpawnPoint : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightChestSpawnPoint();
    virtual void PostLoad() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") FName Region = "Center";
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") TObjectPtr<UCandyFightChestConfig> ConfigOverride;
private:
    void RefreshEditorMarker();
#if WITH_EDITORONLY_DATA
    UPROPERTY() TObjectPtr<UBoxComponent> ChestMarker;
    UPROPERTY() TObjectPtr<UTextRenderComponent> RegionLabel;
#endif
};

USTRUCT(BlueprintType)
struct FCandyFightChestRegion
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Region;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Count = 2;
};

UCLASS(Blueprintable)
class TPSDEMO_API ACandyFightChestSpawner : public AActor
{
    GENERATED_BODY()
public:
    ACandyFightChestSpawner();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") TSubclassOf<ACandyFightChest> ChestClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") TObjectPtr<UCandyFightChestConfig> DefaultConfig;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") TArray<FCandyFightChestRegion> Regions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") bool bUseFixedSeed = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest Spawn") int32 RandomSeed = 12345;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Chest Spawn") TArray<TObjectPtr<ACandyFightChest>> SpawnedChests;
    UFUNCTION(BlueprintCallable, Category="Chest Spawn") void GenerateChests();
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
protected:
    virtual void BeginPlay() override;
private:
    bool bGenerated = false;
};
