// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimeGame : ModuleRules
{
	public TimeGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TimeGame",
			"TimeGame/Variant_Platforming",
			"TimeGame/Variant_Platforming/Animation",
			"TimeGame/Variant_Combat",
			"TimeGame/Variant_Combat/AI",
			"TimeGame/Variant_Combat/Animation",
			"TimeGame/Variant_Combat/Gameplay",
			"TimeGame/Variant_Combat/Interfaces",
			"TimeGame/Variant_Combat/UI",
			"TimeGame/Variant_SideScrolling",
			"TimeGame/Variant_SideScrolling/AI",
			"TimeGame/Variant_SideScrolling/Gameplay",
			"TimeGame/Variant_SideScrolling/Interfaces",
			"TimeGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
