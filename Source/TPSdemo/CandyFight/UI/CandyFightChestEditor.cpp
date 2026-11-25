#if WITH_EDITOR
#include "CandyFightChestInteractionWidget.h"
#include "CandyFightWeaponHUD.h"
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
bool SaveChestWidget(UWidgetBlueprint* BP)
{
    BP->WidgetTree->ForEachWidget([BP](UWidget* Widget)
    {
        if (!BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
            BP->WidgetVariableNameToGuidMap.Add(Widget->GetFName(),FGuid::NewGuid());
    });
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) return false;
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
    auto* Package = BP->GetOutermost();
    return UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
FAutoConsoleCommandWithOutputDevice SetupChestWidgets(TEXT("CandyFight.SetupChestWidgets"),
    TEXT("Create chest interaction Designer layout and append only the missing HUD currency panel."),
    FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Out)
    {
        const TCHAR* Path = TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightChestInteractionWidget");
        auto* BP = LoadObject<UWidgetBlueprint>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
        if (!BP)
        {
            auto* Package = CreatePackage(Path);
            BP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UCandyFightChestInteractionWidget::StaticClass(),Package,
                TEXT("BP_CandyFightChestInteractionWidget"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
            if (BP) FAssetRegistryModule::AssetCreated(BP);
        }
        if (!BP || BP->ParentClass != UCandyFightChestInteractionWidget::StaticClass()) { Out.Log(TEXT("Chest widget has unexpected parent.")); return; }
        if (!BP->WidgetTree->RootWidget)
        {
            BP->Modify(); UCandyFightChestInteractionWidget::BuildDefaultLayout(BP->WidgetTree);
        }
        if (!SaveChestWidget(BP)) { Out.Log(TEXT("Chest widget compile/save failed.")); return; }
        auto* HUD = LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightWeaponHUD"));
        if (!HUD) { Out.Log(TEXT("HUD missing.")); return; }
        if (!HUD->WidgetTree->FindWidget(TEXT("GoldText")))
        {
            HUD->Modify(); UCandyFightWeaponHUD::AddCurrencyLayout(HUD->WidgetTree);
        }
        if (!HUD->WidgetTree->FindWidget(TEXT("GoldText")) || !SaveChestWidget(HUD)) { Out.Log(TEXT("HUD currency layout compile/save failed.")); return; }
        Out.Log(TEXT("Chest widgets ready; existing layouts preserved."));
    }));
}
#endif
