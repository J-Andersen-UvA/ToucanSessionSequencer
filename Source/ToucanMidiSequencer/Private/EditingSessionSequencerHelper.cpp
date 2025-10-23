#include "EditingSessionSequencerHelper.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "LevelSequence.h"
#include "ControlRig.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetTypeCategories.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"

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

    if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
    {
        // Try to find an existing binding for this actor
        FGuid BindingID = LevelSequence->FindBindingFromObject(MeshActor, World);
        if (BindingID.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Reusing existing possessable binding for %s"), *MeshActor->GetName());
        }
        else
        {
            // Fallback: check by label (rare cases)
            for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
            {
                const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
                if (Possessable && Possessable->GetName() == MeshActor->GetActorLabel())
                {
                    BindingID = Binding.GetObjectGuid();
                    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Found existing possessable by name for %s"), *MeshActor->GetName());
                    break;
                }
            }

            // Still nothing? Create it once.
            if (!BindingID.IsValid())
            {
                BindingID = MovieScene->AddPossessable(MeshActor->GetActorLabel(), MeshActor->GetClass());
                LevelSequence->BindPossessableObject(BindingID, *MeshActor, World);
                UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Created new possessable for %s"), *MeshActor->GetName());
            }
        }


        // Set the anim track and length
        AddAnimationTrack(LevelSequence, Animation, BindingID);

        const double Length = Animation->GetPlayLength();
        const double FrameRate = Animation->GetSamplingFrameRate().AsDecimal();
        const int32 NumFrames = static_cast<int32>(Length * FrameRate);
        MovieScene->SetPlaybackRange(0, NumFrames);
    }

    // Add rig if selected
    AddRigToSequence(LevelSequence, Rig);

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Loaded animation '%s' into Level Sequence."), *Animation->GetName());
}

ULevelSequence* FEditingSessionSequencerHelper::CreateOrLoadLevelSequence()
{
    const FString AssetPath = TEXT("/Game/ToucanTemp");
    const FString AssetName  = TEXT("EditingSession_Sequence");
    FString PackagePath = AssetPath / AssetName;
    FString PackageName = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);

    // Make sure /Game path is correct (avoid /Game//Game/)
    PackageName = PackageName.Replace(TEXT("//"), TEXT("/"));

    // Try to find an existing sequence first
    ULevelSequence* Existing = LoadObject<ULevelSequence>(nullptr, *PackageName);
    if (Existing)
    {
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Loaded existing sequence: %s"), *PackageName);
        return Existing;
    }

    // Otherwise create new package and asset
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();

    ULevelSequence* NewSequence = NewObject<ULevelSequence>(
        Package,
        ULevelSequence::StaticClass(),
        *AssetName,
        RF_Public | RF_Standalone
    );

    // Ensure it has a MovieScene
    if (!NewSequence->GetMovieScene())
    {
        UMovieScene* MovieScene = NewObject<UMovieScene>(NewSequence, NAME_None, RF_Transactional);
        NewSequence->Initialize();
        NewSequence->MovieScene = MovieScene;
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Created new MovieScene manually."));
    }

    // Register with the asset registry so it appears in Content Browser
    FAssetRegistryModule::AssetCreated(NewSequence);

    // Mark as dirty so it will be saved
    Package->MarkPackageDirty();
    NewSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Created new Level Sequence: %s"), *PackageName);
    return NewSequence;
}

ULevelSequence* FEditingSessionSequencerHelper::CreateLevelSequenceAsset(const FString& FolderPath, const FString& AssetName)
{
    const FString PackagePath = FolderPath / AssetName;
    FString UniquePackageName, UniqueAssetName;
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

    if (!FPackageName::DoesPackageExist(FolderPath))
    {
        AssetToolsModule.Get().CreateUniqueAssetName(
            FolderPath + TEXT("/") + AssetName,
            TEXT(""),
            UniquePackageName,
            UniqueAssetName
        );
    }

    IAssetTools& AssetTools = AssetToolsModule.Get();
    UObject* NewAsset = AssetTools.CreateAsset(
        *UniqueAssetName,
        *FolderPath,
        ULevelSequence::StaticClass(),
        nullptr
    );

    // We need to add the MovieScene by hand, the factory was removed in some version...
    ULevelSequence* NewSeq = Cast<ULevelSequence>(NewAsset);
    if (NewSeq)
    {
        if (!NewSeq->GetMovieScene())
        {
            UMovieScene* MovieScene = NewObject<UMovieScene>(NewSeq, NAME_None, RF_Transactional);
            NewSeq->Initialize();
            NewSeq->MovieScene = MovieScene;

            UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Created new MovieScene manually."));
        }
    }

    return NewSeq;
}

ASkeletalMeshActor* FEditingSessionSequencerHelper::SpawnOrFindSkeletalMeshActor(
    UWorld* World, TSoftObjectPtr<USkeletalMesh> SkeletalMesh)
{
    if (!World)
        return nullptr;

    USkeletalMesh* TargetMesh = SkeletalMesh.Get();
    ASkeletalMeshActor* MeshActor = nullptr;
    
    for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
    {
        ASkeletalMeshActor* ExistingActor = *It;
        if (!ExistingActor || !ExistingActor->GetSkeletalMeshComponent())
            continue;

        USkeletalMesh* ExistingMesh = ExistingActor->GetSkeletalMeshComponent()->GetSkeletalMeshAsset();
        if (ExistingMesh == TargetMesh)
        {
            MeshActor = ExistingActor;
            UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Reusing SkeletalMeshActor with same mesh: %s"), *ExistingActor->GetName());
            break;
        }
    }

    // Fallback
    if (!MeshActor)
    {
        for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
        {
            if (It->GetName() == TEXT("EditingSession_SkeletalMeshActor"))
            {
                MeshActor = *It;
                UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Reusing EditingSession_SkeletalMeshActor."));
                break;
            }
        }
    }

    if (!MeshActor)
    {
        MeshActor = World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass());
        MeshActor->SetActorLabel(TEXT("EditingSession_SkeletalMeshActor"));
    }

    if (MeshActor && TargetMesh)
    {
        MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(TargetMesh);
    }

    return MeshActor;
}

void FEditingSessionSequencerHelper::AddAnimationTrack(ULevelSequence* LevelSequence, UAnimSequence* Animation, FGuid BindingID)
{
    if (!LevelSequence || !Animation)
        return;

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return;

    // Find the existing binding by ID
    FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingID);
    if (!Binding)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Invalid BindingID when adding animation track."));
        return;
    }

    // Remove old animation tracks bound to this actor only
    TArray<UMovieSceneTrack*> TracksToRemove;
    for (UMovieSceneTrack* Track : Binding->GetTracks())
    {
        if (Cast<UMovieSceneSkeletalAnimationTrack>(Track))
        {
            TracksToRemove.Add(Track);
        }
    }
    for (UMovieSceneTrack* Track : TracksToRemove)
    {
        MovieScene->RemoveTrack(*Track);
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Removed old animation track for binding '%s'."), *Binding->GetName());
    }

    UMovieSceneSkeletalAnimationTrack* AnimTrack = Cast<UMovieSceneSkeletalAnimationTrack>(
        MovieScene->AddTrack(UMovieSceneSkeletalAnimationTrack::StaticClass(), BindingID)
    );

    if (!AnimTrack)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to create animation track."));
        return;
    }

    // Ensure proper frame rate and resolution
    const FFrameRate TickRes = MovieScene->GetTickResolution();
    const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
    const double FrameTickValue = TickRes.AsDecimal() / DisplayRate.AsDecimal();

    UMovieSceneSection* Section = AnimTrack->AddNewAnimation(FFrameNumber(0), Animation);

    if (Section)
    {
        // Adjust section length
        const double LengthSeconds = Animation->GetPlayLength();
        const int32 EndFrame = TickRes.AsFrameNumber(LengthSeconds).Value;

        Section->SetRange(TRange<FFrameNumber>::Inclusive(0, FFrameNumber(EndFrame)));

        // Force track refresh
        AnimTrack->Modify();
        AnimTrack->UpdateEasing();
        MovieScene->Modify();
        Section->Modify();
        LevelSequence->MarkPackageDirty();

        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Added animation '%s' (%f s) to Level Sequence."), *Animation->GetName(), LengthSeconds);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] AddNewAnimation returned null. Possibly invalid binding or uninitialized sequence."));
    }
}

void FEditingSessionSequencerHelper::AddRigToSequence(
    ULevelSequence* LevelSequence, TSoftObjectPtr<UObject> Rig)
{
    if (!LevelSequence || !Rig.IsValid())
        return;

    UObject* RigObject = Rig.Get();
    if (!RigObject)
        return;

    // Determine the ControlRig class from either a ControlRig asset or a ControlRig Blueprint.
    UClass* RigClass = nullptr;
    if (RigObject->IsA(UControlRig::StaticClass()))
    {
        RigClass = RigObject->GetClass();
    }
    else
    {
        FProperty* GenClassProp = RigObject->GetClass()->FindPropertyByName(TEXT("GeneratedClass"));
        if (FObjectProperty* ObjProp = CastField<FObjectProperty>(GenClassProp))
        {
            UObject* GenClassObj = ObjProp->GetObjectPropertyValue_InContainer(RigObject);
            RigClass = Cast<UClass>(GenClassObj);
        }
    }
    if (!RigClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Invalid or unsupported rig asset."));
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return;

    // --- Find the existing SkeletalMeshActor binding in the sequence ---
    FGuid BindingID;
    if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
    {
        for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
        {
            const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
            if (Possessable && Possessable->GetPossessedObjectClass() == ASkeletalMeshActor::StaticClass())
            {
                BindingID = Binding.GetObjectGuid();
                break;
            }
        }
    }
    if (!BindingID.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No SkeletalMeshActor binding found in sequence."));
        return;
    }

    // --- Resolve the actual bound actor instance (the one Sequencer uses) ---
    ASkeletalMeshActor* MeshActor = nullptr;
    {
        TArray<UObject*, TInlineAllocator<1>> BoundObjs;
        UE::UniversalObjectLocator::FResolveParams ResolveParams(World);
        LevelSequence->LocateBoundObjects(BindingID, ResolveParams, BoundObjs);
        if (BoundObjs.Num() > 0)
            MeshActor = Cast<ASkeletalMeshActor>(BoundObjs[0]);
    }
    if (!MeshActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Could not locate bound SkeletalMeshActor instance."));
        return;
    }

    // --- Create or reuse the ControlRig track ---
    FMovieSceneBindingProxy BindingProxy(BindingID, LevelSequence);
    UMovieSceneTrack* RigTrack = UControlRigSequencerEditorLibrary::FindOrCreateControlRigTrack(
        World, LevelSequence, RigClass, BindingProxy, false);

    if (RigTrack)
    {
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Added ControlRig '%s' to Level Sequence binding."),
            *RigObject->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to create ControlRig track."));
    }
}
