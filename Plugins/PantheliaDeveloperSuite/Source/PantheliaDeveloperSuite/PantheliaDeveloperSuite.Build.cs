using UnrealBuildTool;

public class PantheliaDeveloperSuite : ModuleRules
{
    public PantheliaDeveloperSuite(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "ContentBrowser",
            "DataValidation",
            "GameplayTags",
            "InputCore",
            "Json",
            "JsonUtilities",
            "LevelEditor",
            "Projects",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "ToolsetRegistry",
            "UnrealEd"
        });
    }
}
