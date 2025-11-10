#pragma once
#include "CoreMinimal.h"
#include "ControlRig.h"
#include "MidiTypes.h"

class UMovieSceneSequence;

class FToucanMidiRigBinder
{
public:
    static void RegisterRigControls();
    static void BindRigChangeListener();

    // Handle incoming MIDI control input
    static void OnMidiControlInput(const FString& FunctionId, const FMidiControlValue& V);
    static void KeyframeRigControlNow(UControlRig* Rig, const FName& ControlName, float NormalizedValue);
    static void KeyframeRigControlAt(UControlRig* Rig, const FName& ControlName, int32 FrameNumber, float NormalizedValue, UMovieSceneSequence* Sequence);
};
