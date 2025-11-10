#include "ToucanMidiRigBinder.h"

#if WITH_MIDIMAPPER
#include "MidiMappingManager.h"
#endif

#include "ControlRigSequencerEditorLibrary.h"
#include "ControlRig.h"
#include "LevelSequence.h"
#include "Rigs/RigHierarchy.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/SoftObjectPath.h"
#include "Modules/ModuleManager.h"
#include "EditingSessionDelegates.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "ISequencer.h"
#include "SequencerControlSubsystem.h"

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
            Func.Id = FString::Printf(TEXT("Rig.%s"), *ControlName.ToString());
            Func.Label = Func.Id;
            Func.Callback.BindStatic(&FToucanMidiRigBinder::OnMidiControlInput);
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
                M->UnregisterTopic(TEXT("Rig."));

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

void FToucanMidiRigBinder::OnMidiControlInput(const FMidiControlValue& V)
{
    FString RigPath;
    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), RigPath, GEditorPerProjectIni);
    FString RigName = FPaths::GetBaseFilename(RigPath);
 
    FString FunctionId = TEXT("Rig.");

    //const FString& Device, int32 ControlId, float Value, const FString& FunctionId;
    UE_LOG(LogToucanRigBinder, Log, TEXT("Triggered %s from %s:%d = %.3f"), *FunctionId, *V.Device, V.ControlId, V.Value);

    UMovieSceneSequence* Sequence = USequencerControlSubsystem::GetCurrentSequence();
    if (!Sequence)
    {
        UE_LOG(LogToucanRigBinder, Warning,
            TEXT("OnMidiControlInput: No active sequencer found (Device=%s, Control=%d, FuncId=%s)"),
            *V.Device, V.ControlId, *FunctionId);
        return;
    }

    UControlRig* Rig = USequencerControlSubsystem::GetBoundRigFromSequencer(Sequence, RigName);
    if (!Rig)
    {
        UE_LOG(LogToucanRigBinder, Warning,
            TEXT("OnMidiControlInput: Could not resolve rig '%s' (Path=%s, FuncId=%s)"),
            *RigName, *RigPath, *FunctionId);
        return;
    }

    FName ControlName = *FunctionId.RightChop(4); // strip "Rig."
    float Normalized = V.Value; // Is already normalized?
    FToucanMidiRigBinder::KeyframeRigControlNow(Rig, ControlName, Normalized);
}

void FToucanMidiRigBinder::KeyframeRigControlNow(UControlRig* Rig, const FName& ControlName, float NormalizedValue)
{
    UMovieSceneSequence* Seq = USequencerControlSubsystem::GetCurrentSequence();
    const int32 Frame = USequencerControlSubsystem::GetCurrentTimeInFrames();
    KeyframeRigControlAt(Rig, ControlName, Frame, NormalizedValue, Seq);
}

void FToucanMidiRigBinder::KeyframeRigControlAt(
    UControlRig* Rig,
    const FName& ControlName,
    int32 FrameNumber,
    float NormalizedValue,
    UMovieSceneSequence* Sequence)
{
    if (!Rig || !Sequence)
    {
        UE_LOG(LogToucanRigBinder, Warning, TEXT("KeyframeRigControlAt: Invalid input (Rig or Sequence null)"));
        return;
    }

    FRigControlElement* Control = Rig->FindControl(ControlName);
    if (!Control)
    {
        UE_LOG(LogToucanRigBinder, Warning, TEXT("Control '%s' not found on rig '%s'"), *ControlName.ToString(), *Rig->GetName());
        return;
    }

    // Fetch control settings
    const float Min = Control->Settings.MinimumValue.Get<float>();
    const float Max = Control->Settings.MaximumValue.Get<float>();

    // Remap normalized (0–1) to rig control range
    const float MappedValue = FMath::Lerp(Min, Max, NormalizedValue);

    FFrameNumber FrameNum(FrameNumber);

    UE_LOG(LogToucanRigBinder, Log,
        TEXT("Keyframing ControlRig '%s' control '%s' at frame %d (%.3f mapped %.3f → %.3f)"),
        *Rig->GetName(), *ControlName.ToString(), FrameNumber, NormalizedValue, Min, Max);

    if (ULevelSequence* LevelSeq = Cast<ULevelSequence>(Sequence))
    {
        UControlRigSequencerEditorLibrary::SetLocalControlRigFloat(
            LevelSeq,
            Rig,
            ControlName,
            FrameNum,
            MappedValue,
            EMovieSceneTimeUnit::DisplayRate,
            true);

        float Cur = UControlRigSequencerEditorLibrary::GetLocalControlRigFloat(
            LevelSeq,
            Rig,
            ControlName,
            FrameNum,
            EMovieSceneTimeUnit::DisplayRate);

        UE_LOG(LogToucanRigBinder, Log,
            TEXT("Confirmed key on '%s' = %.3f (range %.3f–%.3f)"),
            *ControlName.ToString(), Cur, Min, Max);
    }
}
