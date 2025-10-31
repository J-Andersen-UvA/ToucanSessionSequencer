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
        // Register tab
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            MidiMappingTabName,
            FOnSpawnTab::CreateRaw(this, &FMidiMapperEditorModule::SpawnMidiMappingTab))
            .SetDisplayName(FText::FromString(TEXT("MIDI Mapping")))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        // Add to Toucan menu
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMidiMapperEditorModule::RegisterMenus));

        // Route incoming MIDI actions
        if (auto* Router = FMidiMapperModule::GetRouter())
        {
            Router->OnMidiAction().AddLambda([](FName ActionName, const FMidiControlValue& Value)
                {
                    if (UMidiActionExecutor* Exec = GEditor->GetEditorSubsystem<UMidiActionExecutor>())
                        Exec->ExecuteMappedAction(ActionName, Value);
                });
        }

        auto BindDeviceDelegates = []()
            {
                if (!GEngine) return;
                if (UUnrealMidiSubsystem* MidiSys = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
                {
                    static FDelegateHandle ConnH, DiscH;

                    if (!ConnH.IsValid())
                    {
                        ConnH = MidiSys->OnDeviceConnected.AddLambda([](const FString& Device)
                            {
                                if (UMidiMappingManager* M = UMidiMappingManager::Get())
                                {
                                    FString Rig;
                                    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), Rig, GEditorPerProjectIni);
                                    if (Rig.IsEmpty()) Rig = TEXT("DefaultRig");
                                    else
                                    {
                                        FString ObjPath, ObjName;
                                        Rig.Split(TEXT("."), &ObjPath, &ObjName);
                                        Rig = ObjName.IsEmpty() ? FPaths::GetCleanFilename(Rig) : ObjName;
                                    }
                                    M->Initialize(Device, Rig);
                                    if (auto* Window = SMidiMappingWindow::GetActiveInstance())
                                        Window->RefreshList();
                                    UE_LOG(LogTemp, Log, TEXT("Hot-loaded mapping for '%s'"), *Device);
                                }
                            });
                    }

                    if (!DiscH.IsValid())
                    {
                        DiscH = MidiSys->OnDeviceDisconnected.AddLambda([](const FString& Device)
                            {
                                if (UMidiMappingManager* M = UMidiMappingManager::Get())
                                    M->DeactivateDevice(Device);
                                UE_LOG(LogTemp, Log, TEXT("Device disconnected: %s"), *Device);
                            });
                    }
                }
            };

        if (GEngine)
        {
            BindDeviceDelegates();

            FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([]()
                {
                    FString Rig;
                    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), Rig, GEditorPerProjectIni);
                    if (Rig.IsEmpty()) Rig = TEXT("DefaultRig");
                    else
                    {
                        FString ObjPath, ObjName;
                        Rig.Split(TEXT("."), &ObjPath, &ObjName);
                        Rig = ObjName.IsEmpty() ? FPaths::GetCleanFilename(Rig) : ObjName;
                    }

                    if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
                    {
                        TArray<FUnrealMidiDeviceInfo> Devices;
                        Midi->EnumerateDevices(Devices);

                        if (auto* Router = FMidiMapperModule::GetRouter())
                        {
                            if (UMidiMappingManager* M = UMidiMappingManager::Get())
                            {
                                Router->Init(M);
                                Router->TryBind();
                            }
                        }

                        if (UMidiMappingManager* M = UMidiMappingManager::Get())
                        {
                            for (const auto& D : Devices)
                                if (D.bIsInput)
                                    M->Initialize(D.Name, Rig);
                        }
                    }
                });
        }
        else
        {
            FCoreDelegates::OnPostEngineInit.AddLambda([]()
                {
                    FModuleManager::LoadModuleChecked<FMidiMapperEditorModule>("MidiMapperEditor").StartupModule();
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

        TSharedRef<SDockTab> Tab = SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                Window
            ];

        // Clean up when the tab is closed
        Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)
            {
                if (auto* Router = FMidiMapperModule::GetRouter())
                    Router->CancelLearning();
            }));

        return Tab;
    }
};

IMPLEMENT_MODULE(FMidiMapperEditorModule, MidiMapperEditor)
