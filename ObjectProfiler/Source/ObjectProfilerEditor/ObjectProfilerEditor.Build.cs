//Copyright PsinaDev 2025.

using UnrealBuildTool;

public class ObjectProfilerEditor : ModuleRules
{
	public ObjectProfilerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
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
		});
	}
}