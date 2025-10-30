using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class ToucanSessionSequencer : ModuleRules
{
    public ToucanSessionSequencer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Slate", "SlateCore" });
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "LevelSequence",
            "MovieScene",
            "MovieSceneTracks",
            "ControlRig",
            "ControlRigEditor",
            "Sequencer",
            "LevelSequenceEditor",
            "EditorStyle",
        });

        PrivateDependencyModuleNames.AddRange(new[] {
            "EditorSubsystem",
            "ToolMenus",
            "UnrealEd",
            "AssetRegistry",
            "AssetTools",
            "ContentBrowser",
            "EditorStyle",
            "Projects",
            "InputCore",
            "Kismet",
            "AnimGraphRuntime",
            "EditorScriptingUtilities",
            "MovieSceneTools",
        });
    }
}
