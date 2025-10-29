using UnrealBuildTool;

public class MidiMapper : ModuleRules
{
    public MidiMapper(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "UnrealEd", "Slate", "SlateCore",
            "Json", "JsonUtilities",
        });

        // Dependencies made by Toucan
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "UnrealMidi",
            "ToucanSessionSequencer",
        });
    }
}
