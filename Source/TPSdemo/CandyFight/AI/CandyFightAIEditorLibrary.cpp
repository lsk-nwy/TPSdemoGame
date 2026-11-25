#include "CandyFightAIEditorLibrary.h"

#if WITH_EDITOR
#include "CandyFightPatrolTasks.h"
#include "CandyFightAutonomyTask.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "AI/NavDataGenerator.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"

namespace CandyFightAIEditor
{
	bool Save(UObject* Asset)
	{
		Asset->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset,
			*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}

	UBehaviorTreeGraphNode* Node(UBehaviorTreeGraph* Graph, UClass* InstanceClass, int32 X, int32 Y)
	{
		const bool bComposite = InstanceClass->IsChildOf(UBTCompositeNode::StaticClass());
		UClass* GraphClass = LoadObject<UClass>(nullptr, bComposite ? TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Composite") : TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
		UBehaviorTreeGraphNode* Result = static_cast<UBehaviorTreeGraphNode*>(NewObject<UObject>(Graph, GraphClass, NAME_None, RF_Transactional));
		Result->ClassData = FGraphNodeClassData(InstanceClass, TEXT(""));
		Result->NodePosX = X; Result->NodePosY = Y;
		Result->CreateNewGuid();
		Graph->AddNode(Result, false, false);
		Result->PostPlacedNewNode();
		Result->AllocateDefaultPins();
		return Result;
	}

	void Link(UEdGraphNode* Parent, UEdGraphNode* Child)
	{
		for (UEdGraphPin* Out : Parent->Pins)
			for (UEdGraphPin* In : Child->Pins)
				if (Out->Direction == EGPD_Output && In->Direction == EGPD_Input) { Out->MakeLinkTo(In); return; }
	}
}
#endif

bool UCandyFightAIEditorLibrary::CreatePatrolTree()
{
#if WITH_EDITOR
	using namespace CandyFightAIEditor;
	const TCHAR* BBPath = TEXT("/Game/CandyFight/Blueprints/AI/BB_CandyFightEnemy");
	const TCHAR* BTPath = TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightEnemy");
	// Never overwrite hand-edited behavior trees. Remove only generated assets explicitly to regenerate.
	UBlackboardData* ExistingBB = LoadObject<UBlackboardData>(nullptr, BBPath);
	UBehaviorTree* ExistingBT = LoadObject<UBehaviorTree>(nullptr, BTPath);
	if (ExistingBB || ExistingBT) return ExistingBB && ExistingBT && ExistingBT->RootNode && ExistingBT->BlackboardAsset == ExistingBB;
	UBlackboardData* BB = NewObject<UBlackboardData>(CreatePackage(BBPath), TEXT("BB_CandyFightEnemy"), RF_Public | RF_Standalone);
	auto Key = [BB](FName Name, UClass* Type)
	{
		FBlackboardEntry Entry;
		Entry.EntryName = Name;
		Entry.KeyType = NewObject<UBlackboardKeyType>(BB, Type);
		BB->Keys.Add(Entry);
	};
	Key(TEXT("HomeLocation"), UBlackboardKeyType_Vector::StaticClass());
	Key(TEXT("PatrolLocation"), UBlackboardKeyType_Vector::StaticClass());
	Key(TEXT("HasPatrolLocation"), UBlackboardKeyType_Bool::StaticClass());
	Key(TEXT("PatrolIndex"), UBlackboardKeyType_Int::StaticClass());
	Key(TEXT("PatrolDirection"), UBlackboardKeyType_Int::StaticClass());
	Key(TEXT("WaitDuration"), UBlackboardKeyType_Float::StaticClass());
	Key(TEXT("RetryDelay"), UBlackboardKeyType_Float::StaticClass());
	Key(TEXT("AcceptanceRadius"), UBlackboardKeyType_Float::StaticClass());
	FAssetRegistryModule::AssetCreated(BB);
	UBehaviorTree* BT = NewObject<UBehaviorTree>(CreatePackage(BTPath), TEXT("BT_CandyFightEnemy"), RF_Public | RF_Standalone);
	BT->BlackboardAsset = BB;
	UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(BT, TEXT("BehaviorTree"));
	Graph->Schema = UEdGraphSchema_BehaviorTree::StaticClass();
	BT->BTGraph = Graph;
	Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
	UEdGraphNode* Root = Graph->Nodes[0];
	Root->NodePosX = 400; Root->NodePosY = 0;
	auto Selector = Node(Graph, UBTComposite_Selector::StaticClass(), 400, 160);
	auto Patrol = Node(Graph, UBTComposite_Sequence::StaticClass(), 0, 320);
	auto Retry = Node(Graph, UBTTask_Wait::StaticClass(), 1100, 320);
	CastChecked<UBTTask_Wait>(Retry->NodeInstance)->WaitTime.SetKey(TEXT("RetryDelay"));
	auto Select = Node(Graph, UCandyFightBTTask_SelectPatrolLocation::StaticClass(), -500, 480);
	auto Outcome = Node(Graph, UBTComposite_Selector::StaticClass(), 150, 480);
	auto Success = Node(Graph, UBTComposite_Sequence::StaticClass(), -100, 640);
	auto Failure = Node(Graph, UCandyFightBTTask_HandleMoveFailure::StaticClass(), 700, 640);
	auto Move = Node(Graph, UCandyFightBTTask_MoveToPatrol::StaticClass(), -500, 800);
	auto Wait = Node(Graph, UBTTask_Wait::StaticClass(), -100, 800);
	CastChecked<UBTTask_Wait>(Wait->NodeInstance)->WaitTime.SetKey(TEXT("WaitDuration"));
	auto Complete = Node(Graph, UCandyFightBTTask_CompletePatrolPoint::StaticClass(), 300, 800);
	Link(Root, Selector); Link(Selector, Patrol); Link(Selector, Retry);
	Link(Patrol, Select); Link(Patrol, Outcome);
	Link(Outcome, Success); Link(Outcome, Failure);
	Link(Success, Move); Link(Success, Wait); Link(Success, Complete);
	Graph->UpdateAsset();
	FAssetRegistryModule::AssetCreated(BT);
	return BT->RootNode && Save(BB) && Save(BT);
#else
	return false;
#endif
}

bool UCandyFightAIEditorLibrary::CompileEnemyBlueprint()
{
#if WITH_EDITOR
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/CandyFight/Blueprints/AI/BP_CandyFightEnemy"));
	if (!Blueprint) return false;
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
	return Results.NumErrors == 0 && Blueprint->GeneratedClass &&
		(Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
#else
	return false;
#endif
}

bool UCandyFightAIEditorLibrary::BuildTestNavigation(UObject* WorldContextObject)
{
#if WITH_EDITOR
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || World->IsGameWorld() || World->GetOutermost()->GetName() != TEXT("/Game/CandyFight/Maps/L_CandyFight_AI_Test")) return false;
	ANavMeshBoundsVolume* Bounds = nullptr;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) { Bounds = *It; break; }
	if (!Bounds) Bounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 100), FRotator::ZeroRotator);
	Bounds->SetActorLabel(TEXT("CandyFight_AI_NavBounds"));
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = 4400; Builder->Y = 3400; Builder->Z = 600;
	UActorFactory::CreateBrushForVolumeActor(Bounds, Builder);
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav) return false;
	Nav->OnNavigationBoundsUpdated(Bounds);
	Nav->Build();
	for (ANavigationData* Data : Nav->NavDataSet)
		if (Data && Data->GetGenerator()) Data->GetGenerator()->EnsureBuildCompletion();
	FNavLocation Probe;
	return Nav->ProjectPointToNavigation(FVector(-1000, -500, 0), Probe, FVector(100));
#else
	return false;
#endif
}

AActor* UCandyFightAIEditorLibrary::BeginTestActorSpawn(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FVector Location)
{
#if WITH_EDITOR
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (World && World->WorldType == EWorldType::PIE && World->GetName().Contains(TEXT("L_CandyFight_AI_Test")))
		return World->SpawnActorDeferred<AActor>(ActorClass, FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
#endif
	return nullptr;
}

AActor* UCandyFightAIEditorLibrary::FinishTestActorSpawn(AActor* Actor)
{
#if WITH_EDITOR
	if (Actor && Actor->GetWorld()->WorldType == EWorldType::PIE)
	{
		Actor->FinishSpawning(Actor->GetActorTransform());
		return Actor;
	}
#endif
	return nullptr;
}

bool UCandyFightAIEditorLibrary::CaptureActiveEditorWindow(const FString& Filename)
{
#if WITH_EDITOR
	if (!FSlateApplication::IsInitialized() || Filename != FPaths::GetCleanFilename(Filename)) return false;
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Window.IsValid()) return false;
	TArray<FColor> Pixels;
	FIntVector Size;
	if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size)) return false;
	TArray64<uint8> Compressed;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Compressed);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CandyFight/AI");
	IFileManager::Get().MakeDirectory(*Directory, true);
	return FFileHelper::SaveArrayToFile(Compressed, *(Directory / Filename));
#else
	return false;
#endif
}

bool UCandyFightAIEditorLibrary::CreateAutonomyTree()
{
#if WITH_EDITOR
    using namespace CandyFightAIEditor;
    const TCHAR* BBPath=TEXT("/Game/CandyFight/Blueprints/AI/BB_CandyFightCombat");
    const TCHAR* BTPath=TEXT("/Game/CandyFight/Blueprints/AI/BT_CandyFightCombat");
    auto* ExistingBB=LoadObject<UBlackboardData>(nullptr,BBPath);
    auto* ExistingBT=LoadObject<UBehaviorTree>(nullptr,BTPath);
    if (ExistingBB || ExistingBT) return ExistingBB && ExistingBT && ExistingBT->RootNode && ExistingBT->BlackboardAsset==ExistingBB;
    auto* BB=NewObject<UBlackboardData>(CreatePackage(BBPath),TEXT("BB_CandyFightCombat"),RF_Public|RF_Standalone);
    auto Key=[BB](FName Name,UClass* Type) { FBlackboardEntry E; E.EntryName=Name; E.KeyType=NewObject<UBlackboardKeyType>(BB,Type); BB->Keys.Add(E); };
    Key(TEXT("CombatState"),UBlackboardKeyType_Int::StaticClass());
    Key(TEXT("StateName"),UBlackboardKeyType_String::StaticClass());
    Key(TEXT("TargetVisible"),UBlackboardKeyType_Bool::StaticClass());
    Key(TEXT("LastKnownLocation"),UBlackboardKeyType_Vector::StaticClass());
    Key(TEXT("ReturnAnchor"),UBlackboardKeyType_Vector::StaticClass());
    Key(TEXT("Destination"),UBlackboardKeyType_Vector::StaticClass());
    Key(TEXT("StopReason"),UBlackboardKeyType_String::StaticClass());
    auto* BT=NewObject<UBehaviorTree>(CreatePackage(BTPath),TEXT("BT_CandyFightCombat"),RF_Public|RF_Standalone);
    BT->BlackboardAsset=BB;
    auto* Graph=NewObject<UBehaviorTreeGraph>(BT,TEXT("BehaviorTree"));
    Graph->Schema=UEdGraphSchema_BehaviorTree::StaticClass(); BT->BTGraph=Graph;
    Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    auto* Selector=Node(Graph,UBTComposite_Selector::StaticClass(),900,160);
    Link(Graph->Nodes[0],Selector);
    const TCHAR* Names[]={TEXT("巡逻期间感知"),TEXT("调查最后线索"),TEXT("接近、后退及绕行寻位"),TEXT("停步分组射击"),TEXT("附近移动搜索"),TEXT("忽略线索并归位"),TEXT("控制台测试接管")};
    for (int32 I=0; I<7; ++I)
    {
        auto* N=Node(Graph,UCandyFightAutonomyTask::StaticClass(),I*360,350);
        auto* Task=CastChecked<UCandyFightAutonomyTask>(N->NodeInstance);
        Task->Phase=static_cast<ECandyFightEnemyState>(I); Task->NodeName=Names[I];
        Link(Selector,N);
    }
    Graph->UpdateAsset();
    FAssetRegistryModule::AssetCreated(BB); FAssetRegistryModule::AssetCreated(BT);
    return BT->RootNode && Save(BB) && Save(BT);
#else
    return false;
#endif
}
