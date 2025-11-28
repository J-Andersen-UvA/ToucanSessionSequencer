#include "SequencerControlSubsystem.h"
#if WITH_MIDIMAPPER
#include "MidiMappingManager.h"
#endif

#include "ISequencer.h"
#include "ISequencerModule.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "Misc/QualifiedFrameTime.h"
#include "LevelSequence.h"
#include "ILevelSequenceEditorToolkit.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "ControlRig.h"
#include "MovieSceneBinding.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

float USequencerControlSubsystem::lastTimeStep = 0.0;
bool USequencerControlSubsystem::bSmallStepHeld = false;
bool USequencerControlSubsystem::bLargeStepHeld = false;
TArray<FName> USequencerControlSubsystem::LastTouchedControls;

ISequencer* USequencerControlSubsystem::GetCurrentOpenSequencer()
{
    // Try to get the active LevelSequence from the editor
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
        return nullptr;

    // Find the first LevelSequence editor thats open
    TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
    for (UObject* Asset : EditedAssets)
    {
        if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Asset))
        {
            IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(LevelSequence, false);
            if (const ILevelSequenceEditorToolkit* Toolkit = static_cast<ILevelSequenceEditorToolkit*>(EditorInstance))
            {
                if (TSharedPtr<ISequencer> Seq = Toolkit->GetSequencer())
                {
                    return Seq.Get();
                }
            }
        }
    }

    return nullptr;
}

UMovieSceneSequence* USequencerControlSubsystem::GetCurrentSequence()
{
    if (ISequencer* Seq = GetCurrentOpenSequencer())
        return Seq->GetFocusedMovieSceneSequence();
    return nullptr;
}

int32 USequencerControlSubsystem::GetCurrentTimeInFrames()
{
    if (ISequencer* Seq = GetCurrentOpenSequencer())
        return Seq->GetGlobalTime().Time.FrameNumber.Value;
    return 0;
}

void USequencerControlSubsystem::SetCurrentTimeInFrames(int32 Frame)
{
    if (ISequencer* Seq = GetCurrentOpenSequencer())
    {
        Seq->SetGlobalTime(Frame);
    }
}

void USequencerControlSubsystem::AdvanceByFrames(int32 DeltaFrames)
{
    if (ISequencer* Seq = GetCurrentOpenSequencer())
    {
        const int32 CurrentFrame = Seq->GetGlobalTime().Time.FrameNumber.Value;
        const int32 TargetFrame = CurrentFrame + DeltaFrames;

        FFrameTime NewTime(TargetFrame);
        Seq->SetGlobalTime(NewTime);

        UE_LOG(LogTemp, Log, TEXT("Sequencer advanced by %d frames (now at %d)"), DeltaFrames, TargetFrame);
    }
}

UControlRig* USequencerControlSubsystem::GetBoundRigFromSequencer(
    UMovieSceneSequence* Sequence, const FString& RigName)
{
    if (!Sequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetBoundRigFromSequencer: Sequence is null"));
        return nullptr;
    }

    const UMovieScene* Scene = Sequence->GetMovieScene();
    if (!Scene)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetBoundRigFromSequencer: MovieScene is null"));
        return nullptr;
    }

    ISequencer* Sequencer = USequencerControlSubsystem::GetCurrentOpenSequencer();
    if (!Sequencer)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetBoundRigFromSequencer: No active Sequencer instance"));
        return nullptr;
    }
    Sequencer->ForceEvaluate();

    UE_LOG(LogTemp, Log, TEXT("Searching Sequencer bindings for rig match: '%s'"), *RigName);

    const FMovieSceneSequenceID SequenceID = Sequencer->GetFocusedTemplateID();

    // Iterate all possessables and spawnables; these hold live bindings to runtime objects
    const FMovieSceneRootEvaluationTemplateInstance& RootTemplate = Sequencer->GetEvaluationTemplate();
    const FMovieSceneSequenceID FocusedID = Sequencer->GetFocusedTemplateID();
    const TArray<FMovieSceneBinding>& Bindings = Scene->GetBindings();

    UE_LOG(LogTemp, Log, TEXT("Scene has %d bindings"), Bindings.Num());
    for (const FMovieSceneBinding& B : Bindings)
    {
        UE_LOG(LogTemp, Log, TEXT("  Binding %s -> %s"), *B.GetName(), *B.GetObjectGuid().ToString());
    }
    for (const FMovieSceneBinding& Binding : Bindings)
    {
        const FGuid ObjectGuid = Binding.GetObjectGuid();

        TArrayView<TWeakObjectPtr<UObject>> BoundObjects =
            Sequencer->FindBoundObjects(ObjectGuid, FocusedID);

        for (TWeakObjectPtr<UObject> ObjPtr : BoundObjects)
        {
            if (UControlRig* Rig = Cast<UControlRig>(ObjPtr.Get()))
            {
                FString RigClassName = Rig->GetClass()->GetName();
                FString RigAssetName = Rig->GetName();

                UE_LOG(LogTemp, Log,
                    TEXT("  Found rig candidate: %s (Class=%s, Asset=%s)"),
                    *RigName, *RigClassName, *RigAssetName);

                if (RigClassName.Contains(RigName) || RigAssetName.Contains(RigName))
                {
                    UE_LOG(LogTemp, Log,
                        TEXT("  FOUND match for rig '%s' (Guid=%s)"),
                        *RigName, *ObjectGuid.ToString());
                    return Rig;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("GetBoundRigFromSequencer: No active rig instance found for '%s'"), *RigName);

    // fallback: find active ControlRig instance for the track
    for (const FMovieSceneBinding& Binding : Bindings)
    {
        for (UMovieSceneTrack* Track : Binding.GetTracks())
        {
            if (auto* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
            {
                FString RigAssetName = CRTrack->GetTrackName().ToString();
                if (!RigAssetName.Contains(RigName, ESearchCase::IgnoreCase))
                    continue;

                // Try runtime instance first
                UControlRig* RuntimeRig = CRTrack->GetControlRig();
                if (RuntimeRig && RuntimeRig->IsValidLowLevel())
                {
                    UE_LOG(LogTemp, Log, TEXT("Matched live ControlRig '%s'"), *RuntimeRig->GetName());
                    return RuntimeRig;
                }

                // Try spawned rig from Sequencer player
                if (Sequencer)
                {
                    Sequencer->ForceEvaluate();

                    UMovieSceneSequence* CurrentSequence = Sequencer->GetFocusedMovieSceneSequence();
                    IMovieScenePlayer* Player = Sequencer;

                    // Look for spawnables/possessables that correspond to this track
                    const FGuid ObjectGuid = Binding.GetObjectGuid();
                    TArrayView<TWeakObjectPtr<UObject>> BoundObjects =
                        Player->FindBoundObjects(ObjectGuid, SequenceID);

                    for (TWeakObjectPtr<UObject> ObjPtr : BoundObjects)
                    {
                        if (UControlRig* SpawnedRig = Cast<UControlRig>(ObjPtr.Get()))
                        {
                            UE_LOG(LogTemp, Log,
                                TEXT("Found spawned ControlRig instance '%s' for rig '%s'"),
                                *SpawnedRig->GetName(), *RigName);
                            return SpawnedRig;
                        }
                    }
                }

                // Fallback to template rig only if nothing else found
                if (UControlRig* TemplateRig = CRTrack->GetControlRig())
                {
                    UE_LOG(LogTemp, Log,
                        TEXT("Fallback: using track template ControlRig '%s'"), *TemplateRig->GetName());
                    return TemplateRig;
                }

                UE_LOG(LogTemp, Warning, TEXT("No ControlRig instance found for '%s'"), *RigName);
            }
        }
    }

    return nullptr;
}

void USequencerControlSubsystem::PlaySequencer(bool bPlay)
{
    if (ISequencer* Seq = GetCurrentOpenSequencer())
        Seq->SetPlaybackStatus(bPlay ? EMovieScenePlayerStatus::Playing : EMovieScenePlayerStatus::Stopped);
}

void USequencerControlSubsystem::KeyframeAllRigControlsToZero()
{
    UMovieSceneSequence* Sequence = GetCurrentSequence();
    if (!Sequence)
        return;

    ISequencer* Sequencer = GetCurrentOpenSequencer();
    if (!Sequencer)
        return;

    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (!MovieScene)
        return;

    const int32 Frame = GetCurrentTimeInFrames();
    const FFrameNumber FrameNum(Frame);

    // Look for all ControlRig tracks in the sequence
    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        for (UMovieSceneTrack* Track : Binding.GetTracks())
        {
            auto* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track);
            if (!CRTrack)
                continue;

            FString RigName = CRTrack->GetTrackName().ToString();

            UControlRig* Rig = GetBoundRigFromSequencer(Sequence, RigName);
            if (!Rig || !Rig->IsValidLowLevel())
                continue;

            URigHierarchy* Hier = Rig->GetHierarchy();
            if (!Hier)
                continue;

            const TArray<FRigControlElement*>& Controls = Hier->GetControls();
            if (Controls.IsEmpty())
                continue;

            // ------------------------------------------------------------
            // Zero controls
            // ------------------------------------------------------------
            FTransform T = FTransform::Identity;
            T.SetTranslation(FVector::ZeroVector);
            T.SetScale3D(FVector(1.f));

            for (FRigControlElement* C : Controls)
            {
                if (!C)
                    continue;

                const ERigControlType Type = C->Settings.ControlType;
                const FName Name = C->GetFName();

                switch (Type)
                {
                    case ERigControlType::Float:
                    {

                        UControlRigSequencerEditorLibrary::SetLocalControlRigFloat(
                            Cast<ULevelSequence>(Sequence),
                            Rig, Name, FrameNum,
                            0.f,
                            EMovieSceneTimeUnit::DisplayRate, true);
                        break;
                    }
                    case ERigControlType::Vector2D:
                    {
                        UControlRigSequencerEditorLibrary::SetLocalControlRigVector2D(
                            Cast<ULevelSequence>(Sequence),
                            Rig, Name, FrameNum,
                            FVector2D::ZeroVector,
                            EMovieSceneTimeUnit::DisplayRate, true);
                        break;
                    }
                    case ERigControlType::Position:
                    {
                        UControlRigSequencerEditorLibrary::SetLocalControlRigTransform(
                            Cast<ULevelSequence>(Sequence),
                            Rig, Name, FrameNum,
                            T,
                            EMovieSceneTimeUnit::DisplayRate, true);
                        break;
                    }
                    case ERigControlType::Scale:
                    {
                        UControlRigSequencerEditorLibrary::SetLocalControlRigTransform(
                            Cast<ULevelSequence>(Sequence),
                            Rig, Name, FrameNum,
                            T,
                            EMovieSceneTimeUnit::DisplayRate, true);
                        break;
                    }
                    case ERigControlType::Rotator:
                    {
                        UControlRigSequencerEditorLibrary::SetLocalControlRigRotator(
                            Cast<ULevelSequence>(Sequence),
                            Rig, Name, FrameNum,
                            FRotator::ZeroRotator,
                            EMovieSceneTimeUnit::DisplayRate, true);
                        break;
                    }
                    case ERigControlType::Transform:
                    case ERigControlType::EulerTransform:
                    {
                        FEulerTransform Euler =
                            UControlRigSequencerEditorLibrary::GetLocalControlRigEulerTransform(
                                Cast<ULevelSequence>(Sequence),
                                Rig, Name, FrameNum,
                                EMovieSceneTimeUnit::DisplayRate);

                        Euler.Location = FVector::ZeroVector;
                        Euler.Rotation = FRotator::ZeroRotator;
                        Euler.Scale = FVector::OneVector;

                        UControlRigSequencerEditorLibrary::SetLocalControlRigEulerTransform(
                            Cast<ULevelSequence>(Sequence),
                            Rig, Name, FrameNum,
                            Euler,
                            EMovieSceneTimeUnit::DisplayRate, true);
                        break;
                    }

                    case ERigControlType::Bool:
                    {
                        break;
                    }
                }
            }
        }
    }
}

void USequencerControlSubsystem::KeyframeLastTouchedControls() {}

void USequencerControlSubsystem::SetLastTouchedControls(const TArray<FName>& ControlNames)
{
    LastTouchedControls = ControlNames;
}

const TArray<FName>& USequencerControlSubsystem::GetLastTouchedControls()
{
    return LastTouchedControls;
}

void USequencerControlSubsystem::ClearLastTouchedControls()
{
    LastTouchedControls.Reset();
}

void USequencerControlSubsystem::SetStartTimeToCurrent()
{
    UMovieSceneSequence* Sequence = GetCurrentSequence();
    if (!Sequence)
        return;

    UMovieScene* Scene = Sequence->GetMovieScene();
    if (!Scene)
        return;

    const FFrameNumber Current = FFrameNumber(GetCurrentTimeInFrames());
    const FFrameNumber End = Scene->GetPlaybackRange().GetUpperBoundValue();

    if (Current < End)
    {
        TRange<FFrameNumber> NewRange(
            TRangeBound<FFrameNumber>::Inclusive(Current),
            TRangeBound<FFrameNumber>::Inclusive(End)
        );

        Scene->SetPlaybackRange(NewRange);
    }

    UE_LOG(LogTemp, Log, TEXT("Set Start Time = %d"), Current.Value);
}

void USequencerControlSubsystem::SetEndTimeToCurrent()
{
    UMovieSceneSequence* Sequence = GetCurrentSequence();
    if (!Sequence)
        return;

    UMovieScene* Scene = Sequence->GetMovieScene();
    if (!Scene)
        return;

    const FFrameNumber Current = FFrameNumber(GetCurrentTimeInFrames());
    const FFrameNumber Start = Scene->GetPlaybackRange().GetLowerBoundValue();

    if (Current > Start)
    {
        TRange<FFrameNumber> NewRange(
            TRangeBound<FFrameNumber>::Inclusive(Start),
            TRangeBound<FFrameNumber>::Inclusive(Current)
        );

        Scene->SetPlaybackRange(NewRange);
    }

    UE_LOG(LogTemp, Log, TEXT("Set End Time = %d"), Current.Value);
}

void USequencerControlSubsystem::RegisterSequencerMidiFunctions()
{
#if WITH_MIDIMAPPER
    if (UMidiMappingManager* Manager = UMidiMappingManager::Get())
    {
        auto Bind = [&](const FString& Id, void(*Func)(const FMidiControlValue&))
        {
            FMidiRegisteredFunction F;
            F.Id = Id;
            F.Label = Id;
            F.Callback.BindStatic(Func);
            Manager->RegisterFunction(F.Label, F.Id, F.Callback);
        };

        Bind(TEXT("Seq.TimeControl"), &USequencerControlSubsystem::OnMidi_TimeControl);
        Bind(TEXT("Seq.StepForward"), &USequencerControlSubsystem::OnMidi_StepForward);
        Bind(TEXT("Seq.StepBackward"), &USequencerControlSubsystem::OnMidi_StepBackward);
        Bind(TEXT("Seq.PlayHold"), &USequencerControlSubsystem::OnMidi_PlayHold);
        Bind(TEXT("Seq.KeyframeZero"), &USequencerControlSubsystem::OnMidi_KeyframeAllZero);
        Bind(TEXT("Seq.KeyframeLastTouched"), &USequencerControlSubsystem::OnMidi_KeyframeLastTouched);
        Bind(TEXT("Seq.SmallStepButton"), &USequencerControlSubsystem::OnMidi_SmallStepButton);
        Bind(TEXT("Seq.LargeStepButton"), &USequencerControlSubsystem::OnMidi_LargeStepButton);
        Bind(TEXT("Seq.SetStartTime"), &USequencerControlSubsystem::OnMidi_SetStartTime);
        Bind(TEXT("Seq.SetEndTime"), &USequencerControlSubsystem::OnMidi_SetEndTime);

        UE_LOG(LogTemp, Log, TEXT("Registered global Sequencer MIDI functions"));
    }
#endif
}

void USequencerControlSubsystem::OnMidi_TimeControl(const FMidiControlValue& V)
{
    UE_LOG(LogTemp, Log, TEXT("\tOnMidi_TimeControl val and prev: %f - %f"), V.Value, lastTimeStep);

    float Delta = V.Value - lastTimeStep;

    const int stepSize = bSmallStepHeld ? 1 : (bLargeStepHeld ? 10 : 5);
    // When we reach 127, we should keep going inf
    if (Delta > 0 || V.Value >= 127)
    {
        AdvanceByFrames(+stepSize);
    }
    else
    {
        AdvanceByFrames(-stepSize);
    }

    lastTimeStep = V.Value;
}

// Callback stubs
void USequencerControlSubsystem::OnMidi_SmallStepButton(const FMidiControlValue& V)
{
    bSmallStepHeld = (V.Value >= 0.5f);
}

void USequencerControlSubsystem::OnMidi_LargeStepButton(const FMidiControlValue& V)
{
    bLargeStepHeld = (V.Value >= 0.5f);
}

void USequencerControlSubsystem::OnMidi_StepForward(const FMidiControlValue& V)
{
    if (V.Value > 0.5f)
        StepSequencer(+1);
}

void USequencerControlSubsystem::OnMidi_StepBackward(const FMidiControlValue& V)
{
    if (V.Value > 0.5f)
        StepSequencer(-1);
}

void USequencerControlSubsystem::StepSequencer(int32 Direction)
{
    int32 StepSize = 5;
    if (bSmallStepHeld && !bLargeStepHeld)
        StepSize = 1;
    else if (bLargeStepHeld && !bSmallStepHeld)
        StepSize = 10;
    else if (bSmallStepHeld && bLargeStepHeld)
        StepSize = 20;

    AdvanceByFrames(Direction * StepSize);
    UE_LOG(LogTemp, Log, TEXT("StepSequencer: dir=%d size=%d"), Direction, StepSize);
}

void USequencerControlSubsystem::OnMidi_PlayHold(const FMidiControlValue& V)
{
    PlaySequencer(V.Value > 0.5f);
}

void USequencerControlSubsystem::OnMidi_KeyframeAllZero(const FMidiControlValue& V)
{
    if (V.Value > 0.5f)
        KeyframeAllRigControlsToZero();
}

void USequencerControlSubsystem::OnMidi_KeyframeLastTouched(const FMidiControlValue& V)
{
    if (V.Value > 0.5f)
        KeyframeLastTouchedControls();
}

void USequencerControlSubsystem::OnMidi_SetStartTime(const FMidiControlValue& V)
{
    if (V.Value > 0.5f)
        SetStartTimeToCurrent();
}

void USequencerControlSubsystem::OnMidi_SetEndTime(const FMidiControlValue& V)
{
    if (V.Value > 0.5f)
        SetEndTimeToCurrent();
}