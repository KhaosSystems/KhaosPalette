// Copyright Khaos Games ApS. All Rights Reserved.

using UnrealBuildTool;

public class KhaosPaletteRuntime : ModuleRules
{
	public KhaosPaletteRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
			);
	}
}
