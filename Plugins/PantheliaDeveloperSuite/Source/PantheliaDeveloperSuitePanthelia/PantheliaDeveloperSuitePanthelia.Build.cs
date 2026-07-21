using UnrealBuildTool;

public class PantheliaDeveloperSuitePanthelia : ModuleRules
{
    public PantheliaDeveloperSuitePanthelia(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Core",
            "CoreUObject",
            "DataRegistry",
            "DataValidation",
            "Engine",
            "GameplayAbilities",
            "GameplayTags",
            "Json",
            "PantheliaDeveloperSuite",
            "PantheliaProject",
            "UnrealEd"
        });
    }
}
