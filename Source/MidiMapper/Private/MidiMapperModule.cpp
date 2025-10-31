#include "MidiMapperModule.h"
#include "MidiEventRouter.h"
#include "MidiMappingManager.h"
#include "UnrealMidiSubsystem.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FMidiMapperModule, MidiMapper)

// Add this line before StartupModule / ShutdownModule
static UMidiEventRouter* GRouter = nullptr;

void SafeInit()
{
    if (UUnrealMidiSubsystem* MidiSys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
    {
        static FDelegateHandle ConnH;
        static FDelegateHandle DiscH;

        if (!ConnH.IsValid())
        {
            ConnH = MidiSys->OnDeviceConnected.AddLambda([](const FString& Device)
                {
                    if (UMidiMappingManager* M = UMidiMappingManager::Get())
                    {
                        FString Rig;
                        GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), Rig, GEditorPerProjectIni);
                        if (Rig.IsEmpty()) Rig = TEXT("DefaultRig");

                        M->Initialize(Device, FPaths::GetCleanFilename(Rig));
                        UE_LOG(LogTemp, Log, TEXT("[MidiMapperEditor] Connected: initialized mapping for %s"), *Device);

                        //if (auto* Window = SMidiMappingWindow::GetActiveInstance())
                        //    Window->RefreshList();
                    }
                });
        }

        if (!DiscH.IsValid())
        {
            DiscH = MidiSys->OnDeviceDisconnected.AddLambda([](const FString& Device)
                {
                    if (UMidiMappingManager* M = UMidiMappingManager::Get())
                    {
                        M->DeactivateDevice(Device);
                        UE_LOG(LogTemp, Log, TEXT("[MidiMapperEditor] Disconnected: deactivated %s"), *Device);
                    }
                });
        }
    }
}

void FMidiMapperModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module started."));
    UMidiMappingManager* Manager = UMidiMappingManager::Get();

    GRouter = NewObject<UMidiEventRouter>();
    GRouter->AddToRoot();
    GRouter->Init(Manager);

    if (GEngine)
    {
        SafeInit();
    }
    else
    {
        FCoreDelegates::OnPostEngineInit.AddLambda([this]()
        {
            SafeInit();
        });
    }
}

void FMidiMapperModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MidiMapper module shut down."));
}

UMidiEventRouter* FMidiMapperModule::GetRouter()
{
    return GRouter;
}
