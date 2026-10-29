using System.IO;
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
            "MediaAssets",
            "MediaCompositing",
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
            "DesktopPlatform",
            "EditorStyle",
            "MediaPlate",
            "Projects",
            "InputCore",
            "Kismet",
            "AnimGraphRuntime",
            "EditorScriptingUtilities",
            "MovieSceneTools",
            "ControlRig",
            "ControlRigDeveloper",
            "ControlRigEditor",
            "SequencerScripting",
            "LevelEditor",
            "Sequencer",
            "SequencerAbstraction"
        });

        PublicDefinitions.Add("WITH_MIDIMAPPER=0");

        string handposeTimelineModule = Path.GetFullPath(Path.Combine(
            ModuleDirectory,
            "..",
            "..",
            "..",
            "HandposeTimeline",
            "Source",
            "HandposeTimeline"));
        if (Directory.Exists(handposeTimelineModule))
        {
            PrivateIncludePathModuleNames.Add("HandposeTimeline");
            DynamicallyLoadedModuleNames.Add("HandposeTimeline");
            PublicDefinitions.Add("WITH_HANDPOSE_TIMELINE=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_HANDPOSE_TIMELINE=0");
        }

    }
}
