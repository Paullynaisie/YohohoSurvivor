// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class YohohoSurvivor : ModuleRules
{
	public YohohoSurvivor(ReadOnlyTargetRules Target) : base(Target)
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
			"Niagara",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayTags",
			"Sockets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"YohohoSurvivor",
			"YohohoSurvivor/Variant_Strategy",
			"YohohoSurvivor/Variant_Strategy/UI",
			"YohohoSurvivor/Variant_TwinStick",
			"YohohoSurvivor/Variant_TwinStick/AI",
			"YohohoSurvivor/Variant_TwinStick/Gameplay",
			"YohohoSurvivor/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
