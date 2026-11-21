using UnrealBuildTool;

public class DarkForestEditor : ModuleRules
{
    public DarkForestEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore",
            "UnrealEd", "EditorSubsystem", "MyProject",
            // Menu extensibility for the Tools menu hook.
            "LevelEditor"
        });
    }
}
