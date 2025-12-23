//Copyright PsinaDev 2025.

using UnrealBuildTool;

public class ObjectProfilerEditor : ModuleRules
{
	public ObjectProfilerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		]);

		PrivateDependencyModuleNames.AddRange([
			"Slate",
			"SlateCore",
			"EditorStyle",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"DesktopPlatform",
			"Json",
			"JsonUtilities",
			"ContentBrowser",
			"UMG"
		]);
	}
}