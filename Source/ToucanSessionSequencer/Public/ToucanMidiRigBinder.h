#pragma once
#include "CoreMinimal.h"
#include "ControlRig.h"

class UMovieSceneSequence;

class FToucanMidiRigBinder
{
public:
    static void RegisterRigControls();
    static void BindRigChangeListener();

    // Handle incoming MIDI control input
    static void OnMidiControlInput(const FString& Device, int32 ControlId, float Value, const FString& FunctionId);
    static void KeyframeRigControlNow(UControlRig* Rig, const FName& ControlName, float NormalizedValue);
    static void KeyframeRigControlAt(UControlRig* Rig, const FName& ControlName, int32 FrameNumber, float NormalizedValue, UMovieSceneSequence* Sequence);
};
