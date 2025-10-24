#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"

class ULevelSequence;
class ASkeletalMeshActor;

/**
 * Handles loading/creating Level Sequences and populating them
 * with SkeletalMesh, Rig, and Animation for Toucan editing sessions.
 */
class FEditingSessionSequencerHelper
{
public:
    static void LoadNextAnimation(TSoftObjectPtr<USkeletalMesh> SkeletalMesh,
                                  TSoftObjectPtr<UObject> Rig,
                                  UAnimSequence* Animation);
    static FGuid FindBindingForObject(
        const ULevelSequence* LevelSequence,
        UObject* InObject,
        TSharedPtr<const UE::MovieScene::FSharedPlaybackState> Shared = nullptr);

private:
    // --- Internal helpers ---
    static ULevelSequence* CreateOrLoadLevelSequence();
    static ASkeletalMeshActor* SpawnOrFindSkeletalMeshActor(UWorld* World, TSoftObjectPtr<USkeletalMesh> SkeletalMesh);
    static void AddAnimationTrack(ULevelSequence* LevelSequence, UAnimSequence* Animation, FGuid BindingID, bool bSetAnimRange = true);
    static void AddRigToSequence(ULevelSequence* LevelSequence, TSoftObjectPtr<UObject> Rig);
    static ULevelSequence* CreateLevelSequenceAsset(const FString& FolderPath, const FString& AssetName);

};
