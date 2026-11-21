using UnrealBuildTool;

public class MyProject : ModuleRules
{
    public MyProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Sources include headers relative to the module root (e.g. "Enemy/DarkForestGoblin.h").
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "AIModule", "GameplayTasks", "UMG", "Slate", "SlateCore",
            "NavigationSystem"
        });
        PrivateDependencyModuleNames.Add("MediaAssets");
    }
}
