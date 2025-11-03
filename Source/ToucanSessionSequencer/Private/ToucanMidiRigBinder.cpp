#include "ToucanMidiRigBinder.h"

#if WITH_MIDIMAPPER
#include "MidiMappingManager.h"
#endif

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/SoftObjectPath.h"
#include "Modules/ModuleManager.h"
#include "EditingSessionDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogToucanRigBinder, Log, All);

static UControlRig* LoadControlRigFromPath(const FString& RigPath)
{
    if (RigPath.IsEmpty())
        return nullptr;

    UObject* RigAsset = FSoftObjectPath(RigPath).TryLoad();
    if (!RigAsset)
        return nullptr;

    if (UBlueprint* RigBP = Cast<UBlueprint>(RigAsset))
        return Cast<UControlRig>(RigBP->GeneratedClass ? RigBP->GeneratedClass->GetDefaultObject() : nullptr);

    if (UControlRig* DirectRig = Cast<UControlRig>(RigAsset))
        return DirectRig;

    if (UClass* RigClass = Cast<UClass>(RigAsset))
        return Cast<UControlRig>(RigClass->GetDefaultObject());

    return nullptr;
}

#if WITH_MIDIMAPPER

void FToucanMidiRigBinder::RegisterRigControls()
{
    FString RigPath;
    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), RigPath, GEditorPerProjectIni);
    if (RigPath.IsEmpty())
    {
        UE_LOG(LogToucanRigBinder, Warning, TEXT("No rig path in config"));
        return;
    }

    UControlRig* ControlRig = LoadControlRigFromPath(RigPath);
    if (!ControlRig)
    {
        UE_LOG(LogToucanRigBinder, Warning, TEXT("Failed to load ControlRig from %s"), *RigPath);
        return;
    }

    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy)
    {
        UE_LOG(LogToucanRigBinder, Warning, TEXT("No hierarchy found for %s"), *ControlRig->GetName());
        return;
    }

    const TArray<FRigControlElement*> Controls = Hierarchy->GetControls();
    if (Controls.IsEmpty())
    {
        UE_LOG(LogToucanRigBinder, Warning, TEXT("Rig %s has no controls"), *ControlRig->GetName());
        return;
    }

    if (UMidiMappingManager* Manager = UMidiMappingManager::Get())
    {
        for (const FRigControlElement* CtrlElem : Controls)
        {
            if (!CtrlElem) continue;

            const FName ControlName = CtrlElem->GetFName();

            FMidiRegisteredFunction Func;
            Func.Id = ControlName.ToString();
            Func.Label = FString::Printf(TEXT("Rig.%s"), *ControlName.ToString());
            Func.Callback = nullptr;

            Manager->RegisterFunction(Func.Label, Func.Id, Func.Callback);
        }

        UE_LOG(LogToucanRigBinder, Log,
            TEXT("Registered %d rig controls from %s"),
            Controls.Num(), *ControlRig->GetName());
    }
}

void FToucanMidiRigBinder::BindRigChangeListener()
{
    static bool bBound = false;
    if (bBound)
        return;
    bBound = true;

    GOnRigChanged.AddLambda([](const FString& NewRigPath)
        {
            UE_LOG(LogToucanRigBinder, Log, TEXT("Rig changed → re-registering MIDI functions for %s"), *NewRigPath);
            if (UMidiMappingManager* M = UMidiMappingManager::Get())
                M->ClearRegisteredFunctions();

            FToucanMidiRigBinder::RegisterRigControls();
        });
}

#else

void FToucanMidiRigBinder::RegisterRigControls()
{
    UE_LOG(LogToucanRigBinder, Warning, TEXT("MIDI Mapper module not available"));
}

void FToucanMidiRigBinder::BindRigChangeListener()
{
    UE_LOG(LogToucanRigBinder, Warning, TEXT("MIDI Mapper module not available"));
}

#endif
