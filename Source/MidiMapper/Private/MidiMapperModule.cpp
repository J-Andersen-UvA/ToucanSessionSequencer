#include "MidiMapperModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FMidiMapperModule, MidiMapper)

void FMidiMapperModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module started."));
}

void FMidiMapperModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module shut down."));
}
