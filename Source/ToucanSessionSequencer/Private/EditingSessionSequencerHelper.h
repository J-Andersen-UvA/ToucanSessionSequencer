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

    static ULevelSequence* CreateOrLoadLevelSequence();
    static ULevelSequence* CreateOrLoadLevelSequence(const FString& Path, USkeletalMesh* Mesh, UObject* Rig);
    static void SetActiveSequence(ULevelSequence* Sequence) { ActiveSequence = Sequence; }
    static ULevelSequence* GetActiveSequence() { return ActiveSequence.Get(); }
    static void SetActiveSkeletalMeshComponent(USkeletalMeshComponent* InComp);
    static USkeletalMeshComponent* GetActiveSkeletalMeshComponent();
    static void BakeAndSaveAnimation(const FString& AnimName, const FString& SourceAnimPath);

private:
    static TWeakObjectPtr<ULevelSequence> ActiveSequence;
    static TWeakObjectPtr<USkeletalMeshComponent> ActiveSkeletalMeshComponent;

private:
    // --- Internal helpers ---
    static ASkeletalMeshActor* SpawnOrFindSkeletalMeshActor(UWorld* World, TSoftObjectPtr<USkeletalMesh> SkeletalMesh);
    static void AddAnimationTrack(ULevelSequence* LevelSequence, UAnimSequence* Animation, FGuid BindingID, bool bSetAnimRange = true);
    static void AddRigToSequence(ULevelSequence* LevelSequence, TSoftObjectPtr<UObject> Rig);
    static ULevelSequence* CreateLevelSequenceAsset(const FString& FolderPath, const FString& AssetName);

};
