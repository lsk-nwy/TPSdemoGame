// One-time migration of the original empty HUD Blueprint to an editable Designer tree.
#if WITH_EDITOR
#include "CandyFightWeaponHUD.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
FAutoConsoleCommandWithOutputDevice SetupHUD(
    TEXT("CandyFight.SetupHUD"), TEXT("Populate the empty HUD Designer tree once; never replace an authored layout."),
    FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Out)
    {
        auto* BP = LoadObject<UWidgetBlueprint>(nullptr,
            TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightWeaponHUD.BP_CandyFightWeaponHUD"));
        if (!BP || !BP->WidgetTree) { Out.Log(TEXT("HUD setup failed: missing Blueprint/tree.")); return; }
        if (BP->WidgetTree->RootWidget) { Out.Log(TEXT("HUD already has an authored layout; left unchanged.")); return; }
        BP->Modify();
        BP->WidgetTree->Modify();
        UCandyFightWeaponHUD::BuildDefaultLayout(BP->WidgetTree);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) { Out.Log(TEXT("HUD compilation failed; asset was not saved.")); return; }
        UPackage* Package = BP->GetOutermost();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        const bool Saved = UPackage::SavePackage(Package, BP, *Filename, SaveArgs);
        Out.Log(Saved ? TEXT("HUD Designer layout compiled and saved.") : TEXT("HUD save failed."));
    }));
}
#endif
