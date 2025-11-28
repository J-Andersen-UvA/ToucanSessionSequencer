#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MidiTypes.h"
#include "SequencerControlSubsystem.generated.h"

class ISequencer;
class UMovieSceneSequence;
class UControlRig;

UENUM(BlueprintType)
enum class ESequencerStepMode : uint8
{
    Small,
    Normal,
    Large
};

UCLASS()
class USequencerControlSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()
public:
    static ISequencer* GetCurrentOpenSequencer();
    static UMovieSceneSequence* GetCurrentSequence();
    static int32 GetCurrentTimeInFrames();
    static void SetCurrentTimeInFrames(int32 Frame);
    static void AdvanceByFrames(int32 DeltaFrames);
    static UControlRig* GetBoundRigFromSequencer(UMovieSceneSequence* Sequence, const FString& RigName);

    static void StepSequencer(int32 Direction);
    static void PlaySequencer(bool bPlay);
    static void KeyframeAllRigControlsToZero();
    static void KeyframeLastTouchedControls();
    static void SetStartTimeToCurrent();
    static void SetEndTimeToCurrent();

    // --- Utility for state tracking ---
    static void SetLastTouchedControls(const TArray<FName>& ControlNames);
    static const TArray<FName>& GetLastTouchedControls();
    static void ClearLastTouchedControls();

public:
    static void RegisterSequencerMidiFunctions();

    static void OnMidi_TimeControl(const FMidiControlValue& V);
    static void OnMidi_StepForward(const FMidiControlValue& V);
    static void OnMidi_StepBackward(const FMidiControlValue& V);
    static void OnMidi_PlayHold(const FMidiControlValue& V);
    static void OnMidi_KeyframeAllZero(const FMidiControlValue& V);
    static void OnMidi_KeyframeLastTouched(const FMidiControlValue& V);
    static void OnMidi_SmallStepButton(const FMidiControlValue& V);
    static void OnMidi_LargeStepButton(const FMidiControlValue& V);
    static void OnMidi_SetStartTime(const FMidiControlValue& V);
    static void OnMidi_SetEndTime(const FMidiControlValue& V);


private:
    static float lastTimeStep;
    static bool bSmallStepHeld;
    static bool bLargeStepHeld;
    static TArray<FName> LastTouchedControls;
};
