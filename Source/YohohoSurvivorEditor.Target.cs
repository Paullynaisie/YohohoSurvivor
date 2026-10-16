// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class YohohoSurvivorEditorTarget : TargetRules
{
	public YohohoSurvivorEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("YohohoSurvivor");

		// UE 5.8 exige MSVC 14.50+ : prend le plus récent installé, même si un BuildConfiguration.xml global du PC force un vieux compilateur pour d'autres projets.
		WindowsPlatform.CompilerVersion = "Latest";
	}
}
