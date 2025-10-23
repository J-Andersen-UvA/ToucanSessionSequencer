#include "EditingSessionSequencerHelper.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "MovieScene.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
//#include "MovieSceneSkeletalAnimationSection.h"
//#include "MovieSceneControlRigParameterTrack.h"
//#include "MovieSceneControlRigParameterSection.h"
#include "LevelSequence.h"
//#include "LevelSequenceBindingReferences.h"
//#include "LevelSequenceEditorBlueprintLibrary.h"
#include "ControlRig.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
//#include "BlueprintFunctionLibrary.h"
#include "ControlRigSequencerEditorLibrary.h"


void FEditingSessionSequencerHelper::LoadNextAnimation(
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh,
    TSoftObjectPtr<UObject> Rig,
    UAnimSequence* Animation)
{
    if (!Animation)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Invalid animation asset."));
        return;
    }

    if (!SkeletalMesh.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No Skeletal Mesh selected."));
        return;
    }

    ULevelSequence* LevelSequence = CreateOrLoadLevelSequence();
    if (!LevelSequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to create or load Level Sequence."));
        return;
    }

    // Open in Sequencer
    if (UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
    {
        EditorSubsystem->OpenEditorForAsset(LevelSequence);
    }

    // Get current editor world
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No active world context."));
        return;
    }

    // Spawn or reuse skeletal mesh actor
    ASkeletalMeshActor* MeshActor = SpawnOrFindSkeletalMeshActor(World, SkeletalMesh);
    if (!MeshActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Could not spawn skeletal mesh actor."));
        return;
    }

    // Add animation to the sequence
    AddAnimationTrack(LevelSequence, Animation);

    // Set playback range to match animation length
    if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
    {
        const double Length = Animation->GetPlayLength();
        const double FrameRate = Animation->GetSamplingFrameRate().AsDecimal();
        const int32 NumFrames = static_cast<int32>(Length * FrameRate);
        MovieScene->SetPlaybackRange(0, NumFrames);
    }

    // Add rig if selected
    AddRigToSequence(LevelSequence, Rig);

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Loaded animation '%s' into Level Sequence."), *Animation->GetName());
}

//
// ─── Create Or Load Sequence ─────────────────────────────────────────────────────
//
ULevelSequence* FEditingSessionSequencerHelper::CreateOrLoadLevelSequence()
{
    const FString FolderPath = TEXT("/Game/ToucanTemp");
    const FString AssetName  = TEXT("EditingSession_Sequence");
    const FString FullPath   = FolderPath + TEXT("/") + AssetName;

    // Try to load existing sequence
    if (ULevelSequence* Existing = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *FullPath)))
    {
        return Existing;
    }

    // Create new one if not found
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UObject* NewAsset = AssetTools.CreateAsset(AssetName, FolderPath, ULevelSequence::StaticClass(), nullptr);
    return Cast<ULevelSequence>(NewAsset);
}

//
// ─── Spawn Or Find Skeletal Mesh Actor ───────────────────────────────────────────
//
ASkeletalMeshActor* FEditingSessionSequencerHelper::SpawnOrFindSkeletalMeshActor(
    UWorld* World, TSoftObjectPtr<USkeletalMesh> SkeletalMesh)
{
    ASkeletalMeshActor* MeshActor = nullptr;
    for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
    {
        if (It->GetName() == TEXT("EditingSession_SkeletalMeshActor"))
        {
            MeshActor = *It;
            break;
        }
    }

    if (!MeshActor)
    {
        MeshActor = World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass());
        MeshActor->SetActorLabel(TEXT("EditingSession_SkeletalMeshActor"));
    }

    if (MeshActor && SkeletalMesh.IsValid())
    {
        MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh.Get());
    }

    return MeshActor;
}

//
// ─── Add Animation Track ─────────────────────────────────────────────────────────
//
void FEditingSessionSequencerHelper::AddAnimationTrack(ULevelSequence* LevelSequence, UAnimSequence* Animation)
{
    if (!LevelSequence || !Animation)
        return;

    if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
    {
        // Remove old animation track if exists
        for (UMovieSceneTrack* ExistingTrack : MovieScene->GetTracks())
        {
            if (Cast<UMovieSceneSkeletalAnimationTrack>(ExistingTrack))
            {
                MovieScene->RemoveTrack(*ExistingTrack);
                break;
            }
        }

        UMovieSceneSkeletalAnimationTrack* AnimTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>();
        if (AnimTrack)
        {
            UMovieSceneSkeletalAnimationSection* AnimSection =
                Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->CreateNewSection());

            if (AnimSection)
            {
                AnimTrack->AddSection(*AnimSection);

                AnimSection->Params.Animation = Animation;

                const double Length = Animation->GetPlayLength();
                const double FrameRate = Animation->GetSamplingFrameRate().AsDecimal();
                const int32 NumFrames = static_cast<int32>(Length * FrameRate);
                AnimSection->SetRange(TRange<FFrameNumber>::Inclusive(0, NumFrames));

                UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Added animation '%s' to Level Sequence."), *Animation->GetName());
            }
        }
    }
}

void FEditingSessionSequencerHelper::AddRigToSequence(ULevelSequence* LevelSequence, TSoftObjectPtr<UObject> Rig)
{
    if (!LevelSequence || !Rig.IsValid())
        return;

    UControlRig* ControlRigAsset = Cast<UControlRig>(Rig.Get());
    if (!ControlRigAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Selected asset is not a ControlRig."));
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return;

    // 🔹 Find our skeletal mesh actor (the possessable)
    ASkeletalMeshActor* MeshActor = nullptr;
    for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
    {
        if (It->GetName() == TEXT("EditingSession_SkeletalMeshActor"))
        {
            MeshActor = *It;
            break;
        }
    }

    if (!MeshActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No SkeletalMeshActor found for rig binding."));
        return;
    }

    // 🔹 Find the binding for this actor
    FGuid BindingID = LevelSequence->FindBindingFromObject(MeshActor, World);
    if (!BindingID.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] SkeletalMeshActor not bound to sequence — cannot add rig."));
        return;
    }

    // ✅ Wrap the binding in a proxy
    FMovieSceneBindingProxy BindingProxy(BindingID, LevelSequence);

    // 🔹 Add or find the Control Rig track
    TSubclassOf<UControlRig> RigClass = ControlRigAsset->GetClass();
    UMovieSceneTrack* RigTrack = UControlRigSequencerEditorLibrary::FindOrCreateControlRigTrack(
        World, LevelSequence, RigClass, BindingProxy, false);

    if (RigTrack)
    {
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Added ControlRig '%s' to Level Sequence binding."),
            *ControlRigAsset->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to create ControlRig track."));
    }
}
