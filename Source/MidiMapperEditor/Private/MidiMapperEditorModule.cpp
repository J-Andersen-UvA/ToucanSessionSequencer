#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "MidiActionExecutor.h"
#include "MidiMapperModule.h"
#include "MidiEventRouter.h"
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
