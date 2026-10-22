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
			"Sockets",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore" });

		// YSEditorScriptLibrary (création du landscape de l'île) : les fonctions sont réservées à l'éditeur, mais le code généré
		// par UHT référence ALandscape dans toutes les cibles (sinon le packaging échoue : LANDSCAPE_API inconnu)
		PrivateDependencyModuleNames.Add("Landscape");

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
