#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "MidiActionExecutor.h"
#include "MidiMapperModule.h"
#include "MidiEventRouter.h"
#include "UnrealMidiSubsystem.h"
#include "MidiMappingWindow.h"

static const FName MidiMappingTabName(TEXT("ToucanMidiMapping"));

class FMidiMapperEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // Register the tab spawner
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MidiMappingTabName,
            FOnSpawnTab::CreateRaw(this, &FMidiMapperEditorModule::SpawnMidiMappingTab))
            .SetDisplayName(FText::FromString(TEXT("MIDI Mapping")))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        // Add to the same "Toucan sequencer" menu
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FMidiMapperEditorModule::RegisterMenus));

        if (auto* Router = FMidiMapperModule::GetRouter())
        {
            Router->OnMidiAction().AddLambda([](FName ActionName, const FMidiControlValue& Value)
                {
                    if (UMidiActionExecutor* Exec = GEditor->GetEditorSubsystem<UMidiActionExecutor>())
                    {
                        Exec->ExecuteMappedAction(ActionName, Value);
                    }
                });
        }

        InitializeMappingsFromConfig();

        auto BindDeviceDelegates = []()
            {
                if (!GEngine) return;

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
                                    if (auto* Window = SMidiMappingWindow::GetActiveInstance())
                                    {
                                        Window->RefreshList();
                                    }
                                    UE_LOG(LogTemp, Log, TEXT("Hot-loaded mapping for device '%s'"), *Device);
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
                                    UE_LOG(LogTemp, Log, TEXT("Device disconnected: %s"), *Device);
                                }
                            });
                    }
                }

                FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().AddLambda([](SWindow&, void*)
                    {
                        if (SMidiMappingWindow* W = SMidiMappingWindow::GetActiveInstance())
                        {
                            W->RefreshBindings();
                            W->RefreshList();
                        }
                    });
            };

        if (GEngine) {
            BindDeviceDelegates();
            FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([] {
                if (UMidiMappingManager* M = UMidiMappingManager::Get())
                {
                    FString Rig;
                    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), Rig, GEditorPerProjectIni);
                    if (Rig.IsEmpty())
                    {
                        Rig = TEXT("DefaultRig");
                    }
                    else
                    {
                        // Strip asset path or duplicate suffix if any
                        FString ObjectPath, ObjectName;
                        if (Rig.Split(TEXT("."), &ObjectPath, &ObjectName))
                        {
                            Rig = ObjectName;
                        }
                        else
                        {
                            Rig = FPaths::GetCleanFilename(Rig);
                        }
                    }
                    if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
                    {
                        TArray<FUnrealMidiDeviceInfo> Devices;
                        Midi->EnumerateDevices(Devices);
                        if (auto* Router = FMidiMapperModule::GetRouter())
                        {
                            Router->Init(M); // ensures router knows the mapping manager
                            UE_LOG(LogTemp, Log, TEXT("[MidiMapperEditor] Router initialized with mapping manager"));
                            Router->TryBind(); // ensure delegate actually bound now
                            UE_LOG(LogTemp, Log, TEXT("[MidiMapperEditor] Forced router bind after engine loop complete"));
                        }

                        for (const auto& D : Devices)
                        {
                            if (D.bIsInput)
                            {
                                M->Initialize(D.Name, FPaths::GetCleanFilename(Rig));
                                UE_LOG(LogTemp, Warning, TEXT("[MidiMapperEditor] After init: %d mappings loaded for %s_%s"),
                                    M->GetAll().Num(), *D.Name, *Rig);
                                UE_LOG(LogTemp, Log, TEXT("[MidiMapperEditor] Late-init mapping for %s"), *D.Name);
                            }
                        }
                    }
                }
            });
        }
        else {
            FCoreDelegates::OnPostEngineInit.AddLambda([] {
                FMidiMapperEditorModule& Mod = FModuleManager::LoadModuleChecked<FMidiMapperEditorModule>("MidiMapperEditor");
                Mod.StartupModule(); // re-run initialization now that engine exists
            });
        }
    }

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MidiMappingTabName);
    }


    void InitializeMappingsFromConfig()
    {
        FString LastRig;
        GConfig->GetString(
            TEXT("ToucanEditingSession"),
            TEXT("LastSelectedRig"),
            LastRig,
            GEditorPerProjectIni
        );
        if (!LastRig.IsEmpty())
        {
            // Convert from path to name
            FString ObjectPath, ObjectName;
            if (LastRig.Split(TEXT("."), &ObjectPath, &ObjectName))
            {
                LastRig = ObjectName;
            }
            else
            {
                // Fallback: extract from last path segment if it was just a path
                LastRig = FPaths::GetCleanFilename(LastRig);
            }
        }
        else
        {
            LastRig = TEXT("DefaultRig");
        }

        // Collect all sections in the ini
        TArray<FString> SectionNames;
        GConfig->GetSectionNames(GEditorPerProjectIni, SectionNames);

        // Collect all ToucanMidiController devices
        TArray<FString> DeviceNames;
        for (const FString& Section : SectionNames)
        {
            if (Section.StartsWith(TEXT("ToucanMidiController.Device:")))
            {
                FString DeviceName;
                Section.Split(TEXT("Device:"), nullptr, &DeviceName);
                DeviceNames.Add(DeviceName);
            }
        }

        if (DeviceNames.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("No ToucanMidiController devices found in %s"), *GEditorPerProjectIni);
            return;
        }

        // Initialize the mapping manager for all listed devices
        if (UMidiMappingManager* Manager = UMidiMappingManager::Get())
        {
            for (const FString& Device : DeviceNames)
            {
                Manager->Initialize(Device, LastRig);
                UE_LOG(LogTemp, Log, TEXT("Loaded mapping for device '%s' with rig '%s'"), *Device, *LastRig);
            }
        }
    }
private:
    void RegisterMenus()
    {
        if (!UToolMenus::IsToolMenuUIEnabled())
            return;

        // Extend the existing submenu instead of adding a new one
        if (UToolMenu* ToucanMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.ToucanRoot"))
        {
            FToolMenuSection& Sec = ToucanMenu->AddSection("ToucanMidi", FText::FromString("MIDI Tools"));

            Sec.AddMenuEntry(
                "MidiMapping",
                FText::FromString("MIDI Mapping"),
                FText::FromString("Open the MIDI Mapping tab."),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([] {
                    FGlobalTabmanager::Get()->TryInvokeTab(MidiMappingTabName);
                }))
            );
        }
    }

    TSharedRef<SDockTab> SpawnMidiMappingTab(const FSpawnTabArgs&)
    {
        const TSharedRef<SMidiMappingWindow> Window = SNew(SMidiMappingWindow);

        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                Window
            ];
    }
};

IMPLEMENT_MODULE(FMidiMapperEditorModule, MidiMapperEditor)
