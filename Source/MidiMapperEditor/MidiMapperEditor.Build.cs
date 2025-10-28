using UnrealBuildTool;

public class MidiMapperEditor : ModuleRules
{
    public MidiMapperEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore",
            "UnrealEd", "EditorSubsystem", "InputCore", "ToolMenus",
        });

        // Includes by Toucan
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "MidiMapper", "ToucanSessionSequencer", "UnrealMidi"
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "ControlRig",
        });


        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("EditorStyle");
        }
    }
}
