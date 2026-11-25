// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TPSdemo : ModuleRules
{
	public TPSdemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayTags",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd", "BehaviorTreeEditor", "AIGraph", "AssetRegistry", "UMGEditor"
			});
		}

		PublicIncludePaths.AddRange(new string[] {
			"TPSdemo",
			"TPSdemo/Variant_Platforming",
			"TPSdemo/Variant_Platforming/Animation",
			"TPSdemo/Variant_Combat",
			"TPSdemo/Variant_Combat/AI",
			"TPSdemo/Variant_Combat/Animation",
			"TPSdemo/Variant_Combat/Gameplay",
			"TPSdemo/Variant_Combat/Interfaces",
			"TPSdemo/Variant_Combat/UI",
			"TPSdemo/Variant_SideScrolling",
			"TPSdemo/Variant_SideScrolling/AI",
			"TPSdemo/Variant_SideScrolling/Gameplay",
			"TPSdemo/Variant_SideScrolling/Interfaces",
			"TPSdemo/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
