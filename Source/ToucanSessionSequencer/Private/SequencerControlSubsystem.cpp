#include "SequencerControlSubsystem.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelSequence.h"
#include "ILevelSequenceEditorToolkit.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "ControlRig.h"
#include "MovieSceneBinding.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

ISequencer* USequencerControlSubsystem::GetCurrentOpenSequencer()
{
    // Try to get the active LevelSequence from the editor
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
        return nullptr;

    // Find the first LevelSequence editor that’s open
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

    UMovieScene* Scene = Sequence->GetMovieScene();
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

    UE_LOG(LogTemp, Log, TEXT("Scene has %d bindings"), Scene->GetBindings().Num());
    for (const FMovieSceneBinding& B : Scene->GetBindings())
    {
        UE_LOG(LogTemp, Log, TEXT("  Binding %s -> %s"), *B.GetName(), *B.GetObjectGuid().ToString());
    }
    for (const FMovieSceneBinding& Binding : Scene->GetBindings())
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
    for (const FMovieSceneBinding& Binding : Scene->GetBindings())
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
