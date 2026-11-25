#if WITH_EDITOR
#include "CandyFightEnemyHealthWidget.h"
#include "CandyFight/AI/CandyFightEnemyCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetComponent.h"
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
bool SaveHealthAsset(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) return false;
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
    auto* Package = BP->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package, BP, *Filename, Args);
}

FAutoConsoleCommandWithOutputDevice SetupEnemyHealth(TEXT("CandyFight.SetupEnemyHealth"),
    TEXT("Create the enemy health Designer asset and assign it to the enemy Blueprint; preserve existing visuals."),
    FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Out)
    {
        const TCHAR* Path = TEXT("/Game/CandyFight/Blueprints/UI/BP_CandyFightEnemyHealthBar");
        auto* Widget = LoadObject<UWidgetBlueprint>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
        bool bCreated = false;
        if (!Widget)
        {
            auto* Package = CreatePackage(Path);
            Widget = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UCandyFightEnemyHealthWidget::StaticClass(),
                Package, TEXT("BP_CandyFightEnemyHealthBar"), BPTYPE_Normal, UWidgetBlueprint::StaticClass(),
                UWidgetBlueprintGeneratedClass::StaticClass()));
            if (!Widget) { Out.Log(TEXT("Enemy health: failed to create widget Blueprint.")); return; }
            FAssetRegistryModule::AssetCreated(Widget);
            bCreated = true;
        }
        if (Widget->ParentClass != UCandyFightEnemyHealthWidget::StaticClass())
            { Out.Log(TEXT("Enemy health: unexpected widget parent; no changes made.")); return; }
        if (bCreated || !Widget->WidgetTree->RootWidget)
        {
            UCandyFightEnemyHealthWidget::BuildDefaultLayout(Widget->WidgetTree);
            if (!SaveHealthAsset(Widget)) { Out.Log(TEXT("Enemy health: widget compile/save failed.")); return; }
        }
        auto* EnemyBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/CandyFight/Blueprints/AI/BP_CandyFightEnemy"));
        auto* Enemy = EnemyBP && EnemyBP->GeneratedClass ? Cast<ACandyFightEnemyCharacter>(EnemyBP->GeneratedClass->GetDefaultObject()) : nullptr;
        if (!Enemy) { Out.Log(TEXT("Enemy health: enemy Blueprint not found.")); return; }
        const auto CurrentClass = Enemy->HealthWidget->GetWidgetClass();
        if (!CurrentClass || CurrentClass == UCandyFightEnemyHealthWidget::StaticClass())
        {
            EnemyBP->Modify(); Enemy->HealthWidget->Modify();
            Enemy->HealthWidget->SetWidgetClass(Widget->GeneratedClass.Get());
            if (!SaveHealthAsset(EnemyBP)) { Out.Log(TEXT("Enemy health: enemy Blueprint compile/save failed.")); return; }
        }
        Out.Log(TEXT("Enemy health assets ready; existing authored layouts preserved."));
    }));
}
#endif
