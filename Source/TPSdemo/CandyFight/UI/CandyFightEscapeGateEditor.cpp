#if WITH_EDITOR
#include "CandyFightEscapeGateWidget.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
FAutoConsoleCommandWithOutputDevice SetupEscapeWidget(TEXT("CandyFight.SetupEscapeGateWidget"),
    TEXT("Create the independent escape gate Designer widget, preserving any existing layout."),
    FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Out)
    {
        const TCHAR* Path=TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightEscapeGateWidget");
        auto* BP=LoadObject<UWidgetBlueprint>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
        if (!BP)
        {
            auto* Package=CreatePackage(Path);
            BP=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UCandyFightEscapeGateWidget::StaticClass(),Package,
                TEXT("BP_CandyFightEscapeGateWidget"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
            if (BP) FAssetRegistryModule::AssetCreated(BP);
        }
        if (!BP || BP->ParentClass!=UCandyFightEscapeGateWidget::StaticClass()) { Out.Log(TEXT("Unexpected escape widget parent.")); return; }
        if (!BP->WidgetTree->RootWidget) { BP->Modify(); UCandyFightEscapeGateWidget::BuildDefaultLayout(BP->WidgetTree); }
        BP->WidgetTree->ForEachWidget([BP](UWidget* Widget)
        {
            if (!BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) BP->WidgetVariableNameToGuidMap.Add(Widget->GetFName(),FGuid::NewGuid());
        });
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status==BS_Error) { Out.Log(TEXT("Escape widget compilation failed.")); return; }
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        auto* Package=BP->GetOutermost();
        const bool Saved=UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
        Out.Log(Saved ? TEXT("Escape gate widget saved; authored layout preserved.") : TEXT("Escape gate widget save failed."));
    }));
}
#endif
