#include "CandyFightChest.h"
#include "CandyFightSearchComponent.h"
#include "CandyFight/CandyFightCharacter.h"
#include "CandyFight/CandyFightPlayerState.h"
#include "CandyFight/Weapons/CandyFightCombatComponent.h"
#include "CandyFight/Weapons/CandyFightWeapon.h"
#include "CandyFight/UI/CandyFightChestInteractionWidget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/TextRenderComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UCandyFightChestConfig::Validate(FString& Error) const
{
    if (!FMath::IsFinite(SearchSeconds) || SearchSeconds <= 0 || !FMath::IsFinite(InteractionDistance) || InteractionDistance <= 0 ||
        !FMath::IsFinite(MaxHeightDifference) || MaxHeightDifference < 0 || MinGold < 0 || MaxGold < MinGold)
    { Error = TEXT("Chest search time/range must be positive and finite, height nonnegative, and 0 <= MinGold <= MaxGold."); return false; }
    return true;
}
#if WITH_EDITOR
EDataValidationResult UCandyFightChestConfig::IsDataValid(FDataValidationContext& Context) const
{
    const auto Parent = Super::IsDataValid(Context); FString Error;
    if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
    return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

ACandyFightChest::ACandyFightChest()
{
    PrimaryActorTick.bCanEverTick = true; SetCanBeDamaged(false);
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision")); Collision->SetupAttachment(Root);
    Collision->SetBoxExtent(FVector(40,30,35)); Collision->SetRelativeLocation(FVector(0,0,35));
    Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->SetGenerateOverlapEvents(false);
    Collision->bDynamicObstacle = true; Collision->SetAreaClassOverride(UNavArea_Null::StaticClass());
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(Root);
    Body->SetRelativeLocation(FVector(0,0,30)); Body->SetRelativeScale3D(FVector(.8,.6,.6));
    LidHinge = CreateDefaultSubobject<USceneComponent>(TEXT("LidHinge")); LidHinge->SetupAttachment(Root);
    LidHinge->SetRelativeLocation(FVector(0,-30,60));
    Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid")); Lid->SetupAttachment(LidHinge);
    Lid->SetRelativeLocation(FVector(0,30,5)); Lid->SetRelativeScale3D(FVector(.8,.6,.1));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    for (auto* Mesh : {Body.Get(),Lid.Get()})
    {
        Mesh->SetStaticMesh(Cube.Object); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false); Mesh->SetCanEverAffectNavigation(false);
    }
    InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint")); InteractionPoint->SetupAttachment(Root);
    InteractionPoint->SetRelativeLocation(FVector(0,0,50));
    InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget")); InteractionWidget->SetupAttachment(Root);
    InteractionWidget->SetRelativeLocation(FVector(0,0,100)); InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
    InteractionWidget->SetDrawSize(FVector2D(160,112)); InteractionWidget->SetPivot(FVector2D(.5,1));
    InteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); InteractionWidget->SetCanEverAffectNavigation(false);
    InteractionWidget->SetWidgetClass(UCandyFightChestInteractionWidget::StaticClass()); InteractionWidget->SetVisibility(false);
}
const UCandyFightChestConfig* ACandyFightChest::GetConfig() const { return Config ? Config.Get() : GetDefault<UCandyFightChestConfig>(); }
void ACandyFightChest::BeginPlay()
{
    Super::BeginPlay();
    ClosedLidRotation = LidHinge->GetRelativeRotation();
    FRandomStream Random; if (LootSeed) Random.Initialize(LootSeed); else Random.GenerateNewSeed();
    FString Error;
    if (GetConfig()->Validate(Error))
    {
        const uint32 Range = uint32(int64(GetConfig()->MaxGold)-GetConfig()->MinGold+1);
        const uint32 Threshold = uint32((uint64(1) << 32) % Range);
        uint32 Sample;
        do { Sample = Random.GetUnsignedInt(); } while (Sample < Threshold);
        Reward = GetConfig()->MinGold + int32(Sample % Range);
    }
    InteractionWidget->InitWidget(); UpdatePresentation();
}
bool ACandyFightChest::IsInReach(const ACandyFightCharacter* Player) const
{
    FString Error;
    if (!IsValid(Player) || IsActorBeingDestroyed() || !GetConfig()->Validate(Error)) return false;
    const FVector Destination = InteractionPoint->GetComponentLocation();
    const FVector Origin = Player->GetActorLocation();
    if (FVector::DistSquared2D(Origin,Destination) > FMath::Square(GetConfig()->InteractionDistance) ||
        FMath::Abs(Origin.Z-Destination.Z) > GetConfig()->MaxHeightDifference) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightChestReach),false,Player);
    if (Player->Combat->EquippedWeapon) Params.AddIgnoredActor(Player->Combat->EquippedWeapon.Get());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Origin,Destination,ECC_Visibility,Params) || Hit.GetActor() == this;
}
bool ACandyFightChest::TryClaim(ACandyFightCharacter* Player)
{
    if (State != ECandyFightChestState::Unsearched || !IsInReach(Player)) return false;
    Claimant = Player; State = ECandyFightChestState::Searching; return true;
}
void ACandyFightChest::ReleaseClaim(ACandyFightCharacter* Player)
{
    if (State == ECandyFightChestState::Searching && Claimant.Get() == Player) { Claimant.Reset(); State = ECandyFightChestState::Unsearched; }
}
bool ACandyFightChest::CompleteSearch(ACandyFightCharacter* Player)
{
    auto* Wallet = IsValid(Player) ? Player->GetPlayerState<ACandyFightPlayerState>() : nullptr;
    if (!Wallet || State != ECandyFightChestState::Searching || Claimant.Get() != Player || !IsInReach(Player)) return false;
    // Commit before any Blueprint/currency callbacks can reenter.
    State = ECandyFightChestState::Searched; Claimant.Reset(); GrantedGold = Reward;
    OpenStart = GetWorld()->GetTimeSeconds(); bInterruptedFeedback = false;
    if (auto* Widget = Cast<UCandyFightChestInteractionWidget>(InteractionWidget->GetUserWidgetObject()))
        FeedbackEnd = OpenStart + FMath::Max(0.f,Widget->RewardSeconds);
    Wallet->AddGold(Reward); OnLooted.Broadcast(Player,Reward); UpdatePresentation(); return true;
}
void ACandyFightChest::ShowInterrupted()
{
    bInterruptedFeedback = true;
    if (auto* Widget = Cast<UCandyFightChestInteractionWidget>(InteractionWidget->GetUserWidgetObject()))
        FeedbackEnd = GetWorld()->GetTimeSeconds() + FMath::Max(0.f,Widget->InterruptedSeconds);
}
void ACandyFightChest::UpdatePresentation()
{
    auto* PC = UGameplayStatics::GetPlayerController(this,0);
    auto* Player = PC ? Cast<ACandyFightCharacter>(PC->GetPawn()) : nullptr;
    auto* Search = Player ? Player->Search.Get() : nullptr;
    const bool bFeedback = GetWorld()->GetTimeSeconds() < FeedbackEnd;
    FVector2D Screen;
    int32 Width=0,Height=0; if (PC) PC->GetViewportSize(Width,Height);
    bool bVisible = PC && PC->IsLocalController() && Search && (Search->Target == this || bFeedback) &&
        (State != ECandyFightChestState::Searched || bFeedback) && IsInReach(Player) &&
        PC->ProjectWorldLocationToScreen(InteractionWidget->GetComponentLocation(),Screen,true) &&
        Screen.X >= 0 && Screen.Y >= 0 && Screen.X <= Width && Screen.Y <= Height;
    if (bVisible)
    {
        FVector CameraLocation; FRotator CameraRotation; PC->GetPlayerViewPoint(CameraLocation,CameraRotation);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CandyFightChestPromptOcclusion),false,Player);
        if (Player->Combat->EquippedWeapon) Params.AddIgnoredActor(Player->Combat->EquippedWeapon.Get());
        FHitResult Hit;
        bVisible = !GetWorld()->LineTraceSingleByChannel(Hit,CameraLocation,InteractionWidget->GetComponentLocation(),ECC_Visibility,Params) || Hit.GetActor() == this;
    }
    InteractionWidget->SetVisibility(bVisible);
    if (!bVisible) return;
    if (auto* Widget = Cast<UCandyFightChestInteractionWidget>(InteractionWidget->GetUserWidgetObject()))
    {
        const bool bActive = Search->bSearching && Search->Target == this;
        const auto Mode = State == ECandyFightChestState::Searched ? ECandyFightChestPrompt::Looted :
            bActive ? ECandyFightChestPrompt::Searching : bFeedback && bInterruptedFeedback ? ECandyFightChestPrompt::Interrupted : ECandyFightChestPrompt::Ready;
        Widget->SetDisplay(Mode,State == ECandyFightChestState::Searched ? 1.f : bActive ? Search->GetSearchProgress() : 0.f,Search->GetInteractKeyText(),GrantedGold);
    }
}
void ACandyFightChest::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (State == ECandyFightChestState::Searching && !Claimant.IsValid()) { State = ECandyFightChestState::Unsearched; }
    if (State == ECandyFightChestState::Searched)
    {
        const float Alpha = OpenSeconds > 0 && FMath::IsFinite(OpenSeconds) ? FMath::Clamp(float((GetWorld()->GetTimeSeconds()-OpenStart)/OpenSeconds),0.f,1.f) : 1.f;
        LidHinge->SetRelativeRotation(ClosedLidRotation + FRotator(0,0,-OpenAngle*Alpha));
    }
    UpdatePresentation();
}
void ACandyFightChest::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Player = Claimant.Get(); Player && Player->Search) Player->Search->CancelSearch();
    Super::EndPlay(Reason);
}
ACandyFightChestSpawnPoint::ACandyFightChestSpawnPoint()
{
    auto* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Marker")); SetRootComponent(Arrow);
    Arrow->SetHiddenInGame(true); Arrow->ArrowColor = FColor::Cyan; Arrow->SetCanEverAffectNavigation(false);
    // Keep the original root/name so saved candidate transforms and references remain intact.
    Arrow->SetVisibility(false);
#if WITH_EDITORONLY_DATA
    ChestMarker = CreateEditorOnlyDefaultSubobject<UBoxComponent>(TEXT("ChestMarker"));
    ChestMarker->SetupAttachment(Arrow);
    ChestMarker->SetBoxExtent(FVector(40,30,35)); ChestMarker->SetRelativeLocation(FVector(0,0,35));
    ChestMarker->SetLineThickness(4.f); ChestMarker->ShapeColor = FColor(255,210,40);
    ChestMarker->SetHiddenInGame(true); ChestMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChestMarker->SetGenerateOverlapEvents(false); ChestMarker->SetCanEverAffectNavigation(false);
    RegionLabel = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("RegionLabel"));
    RegionLabel->SetupAttachment(Arrow); RegionLabel->SetRelativeLocation(FVector(0,0,85));
    RegionLabel->SetRelativeRotation(FRotator(90,0,0));
    RegionLabel->SetHorizontalAlignment(EHTA_Center); RegionLabel->SetVerticalAlignment(EVRTA_TextCenter);
    RegionLabel->SetWorldSize(28.f); RegionLabel->SetText(FText::FromString(TEXT("CHEST\nCenter")));
    RegionLabel->SetTextRenderColor(FColor(255,210,40)); RegionLabel->SetHiddenInGame(true);
    RegionLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision); RegionLabel->SetCanEverAffectNavigation(false);
    RegionLabel->SetCastShadow(false);
#endif
}
void ACandyFightChestSpawnPoint::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshEditorMarker();
}
void ACandyFightChestSpawnPoint::PostLoad()
{
    Super::PostLoad();
    RefreshEditorMarker();
}
void ACandyFightChestSpawnPoint::RefreshEditorMarker()
{
#if WITH_EDITORONLY_DATA
    if (auto* Arrow = Cast<UArrowComponent>(GetRootComponent())) Arrow->SetVisibility(false);
    const FColor Color = bEnabled ? FColor(255,210,40) : FColor(125,125,125);
    if (ChestMarker) { ChestMarker->ShapeColor = Color; ChestMarker->MarkRenderStateDirty(); }
    if (RegionLabel)
    {
        RegionLabel->SetTextRenderColor(Color);
        RegionLabel->SetText(FText::FromString(FString::Printf(TEXT("CHEST%s\n%s"),bEnabled ? TEXT("") : TEXT(" (OFF)"),*Region.ToString())));
    }
#endif
}
ACandyFightChestSpawner::ACandyFightChestSpawner()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root"))); ChestClass = ACandyFightChest::StaticClass();
}
void ACandyFightChestSpawner::BeginPlay() { Super::BeginPlay(); GenerateChests(); }
void ACandyFightChestSpawner::GenerateChests()
{
    if (bGenerated || !GetWorld() || !GetWorld()->IsGameWorld() || !ChestClass) return;
    bGenerated = true;
    FRandomStream Random; if (bUseFixedSeed) Random.Initialize(RandomSeed); else Random.GenerateNewSeed();
    TSet<FName> Handled;
    for (const auto& Row : Regions)
    {
        if (Handled.Contains(Row.Region)) continue; Handled.Add(Row.Region);
        TArray<ACandyFightChestSpawnPoint*> Points;
        for (TActorIterator<ACandyFightChestSpawnPoint> It(GetWorld()); It; ++It)
            if (It->bEnabled && It->Region == Row.Region) Points.Add(*It);
        Points.Sort([](const auto& A,const auto& B) { return A.GetName() < B.GetName(); });
        if (Row.Count < 0 || Row.Count > Points.Num())
            UE_LOG(LogTemp,Warning,TEXT("Chest region %s requests %d boxes from %d enabled points; runtime clamps the count."),*Row.Region.ToString(),Row.Count,Points.Num());
        const int32 Count = FMath::Clamp(Row.Count,0,Points.Num());
        for (int32 Index=0; Index<Count; ++Index)
        {
            const int32 Pick = Random.RandRange(Index,Points.Num()-1); Points.Swap(Index,Pick);
            auto* Point = Points[Index];
            auto* Chest = GetWorld()->SpawnActorDeferred<ACandyFightChest>(ChestClass,Point->GetActorTransform(),this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Chest) continue;
            Chest->Config = Point->ConfigOverride ? Point->ConfigOverride : DefaultConfig;
            Chest->LootSeed = Random.RandRange(1,MAX_int32-1);
            Chest->FinishSpawning(Point->GetActorTransform()); SpawnedChests.Add(Chest);
        }
    }
}
#if WITH_EDITOR
EDataValidationResult ACandyFightChest::IsDataValid(FDataValidationContext& Context) const
{
    const auto Parent = Super::IsDataValid(Context); FString Error;
    if (!GetConfig()->Validate(Error) || !FMath::IsFinite(OpenSeconds) || OpenSeconds < 0 || !FMath::IsFinite(OpenAngle))
    { Context.AddError(FText::FromString(Error.IsEmpty() ? TEXT("Invalid lid animation settings.") : Error)); return EDataValidationResult::Invalid; }
    return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
EDataValidationResult ACandyFightChestSpawner::IsDataValid(FDataValidationContext& Context) const
{
    Super::IsDataValid(Context); bool bValid = ChestClass != nullptr; TSet<FName> Seen;
    if (!ChestClass) Context.AddError(FText::FromString(TEXT("Assign a chest class.")));
    for (const auto& Row : Regions)
    {
        int32 Available = 0;
        if (GetWorld()) for (TActorIterator<ACandyFightChestSpawnPoint> It(GetWorld()); It; ++It) if (It->bEnabled && It->Region == Row.Region) ++Available;
        if (Row.Region.IsNone() || Seen.Contains(Row.Region) || Row.Count < 0 || Row.Count > Available)
        { Context.AddError(FText::FromString(FString::Printf(TEXT("Invalid/duplicate chest region %s: %d requested, %d available."),*Row.Region.ToString(),Row.Count,Available))); bValid=false; }
        Seen.Add(Row.Region);
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
