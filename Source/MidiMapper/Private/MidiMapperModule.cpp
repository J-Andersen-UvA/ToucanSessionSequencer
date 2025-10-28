#include "MidiMapperModule.h"
#include "MidiEventRouter.h"
#include "MidiMappingManager.h"
#include "UnrealMidiSubsystem.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FMidiMapperModule, MidiMapper)

// Add this line before StartupModule / ShutdownModule
static UMidiEventRouter* GRouter = nullptr;

void FMidiMapperModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module started."));
    UMidiMappingManager* Manager = UMidiMappingManager::Get();

    GRouter = NewObject<UMidiEventRouter>();
    GRouter->AddToRoot();
    GRouter->Init(Manager);
}

void FMidiMapperModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module shut down."));
}

UMidiEventRouter* FMidiMapperModule::GetRouter()
{
    return GRouter;
}