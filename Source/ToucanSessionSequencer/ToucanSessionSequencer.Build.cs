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
            "Sequencer"
        });

        bool bHasMidiMapper = Directory.Exists(Path.Combine(ModuleDirectory, "../../../UnrealMidi/Source/MidiMapper"));
        PublicDefinitions.Add("WITH_MIDIMAPPER=" + (bHasMidiMapper ? "1" : "0"));

        if (bHasMidiMapper)
        {
            PrivateDependencyModuleNames.Add("MidiMapper");
            PrivateDependencyModuleNames.Add("MidiMapperEditor");
        }

        bool bHasSequencerAbstraction = Directory.Exists(Path.Combine(ModuleDirectory, "../../../SequencerAbstraction/Source/SequencerAbstraction"));
        PublicDefinitions.Add("WITH_SEQUENCER_ABSTRACTION=" + (bHasSequencerAbstraction ? "1" : "0"));

        if (bHasSequencerAbstraction)
        {
            PrivateDependencyModuleNames.Add("SequencerAbstraction");
        }
    }
}
