// Copyright Khaos Systems. All Rights Reserved.

using UnrealBuildTool;

public class KhaosPaletteEditor : ModuleRules
{
	public KhaosPaletteEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"KhaosPaletteRuntime",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"AssetDefinition",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"EditorWidgets",
				"InputCore",
				"LevelEditor",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"WorkspaceMenuStructure",
			}
			);
	}
}
