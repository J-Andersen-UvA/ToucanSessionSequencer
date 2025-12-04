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
#include "MovieScenePossessable.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "SequencerSettings.h"
#include "SequencerTools.h"
#include "Animation/AnimSequence.h"
#include "Factories/AnimSequenceFactory.h"
#include "EditorAssetLibrary.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneSequence.h"
#include "Exporters/AnimSeqExportOption.h"
#include "OutputHelper.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieSceneSequencePlayer.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "MovieSceneSequenceID.h"
#include "Animation/AnimationSettings.h"
#include "Runtime/Launch/Resources/Version.h"

TWeakObjectPtr<ULevelSequence> FEditingSessionSequencerHelper::ActiveSequence;
TWeakObjectPtr<USkeletalMeshComponent> FEditingSessionSequencerHelper::ActiveSkeletalMeshComponent;
TWeakObjectPtr<UControlRig> FEditingSessionSequencerHelper::ActiveRig;

void setLooping(ULevelSequence* LevelSequence)
{
    if (UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
    {
        if (IAssetEditorInstance* Inst = EditorSubsystem->FindEditorForAsset(LevelSequence, /*bFocusIfOpen*/false))
        {
            if (ILevelSequenceEditorToolkit* Toolkit = static_cast<ILevelSequenceEditorToolkit*>(Inst))
            {
                if (TSharedPtr<ISequencer> Seq = Toolkit->GetSequencer())
                {
                    if (USequencerSettings* Settings = Seq->GetSequencerSettings())
                    {
                        Settings->SetLoopMode(ESequencerLoopMode(2));
                    }
                }
            }
        }
    }
}

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
        setLooping(LevelSequence);
    }

    RemoveRigFromSequence(LevelSequence);

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

    if (MeshActor && MeshActor->GetSkeletalMeshComponent())
    {
        SetActiveSkeletalMeshComponent(MeshActor->GetSkeletalMeshComponent());
    }

    if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
    {
        MovieScene->SetDisplayRate(Animation->GetSamplingFrameRate());
        MovieScene->SetTickResolutionDirectly(Animation->GetSamplingFrameRate());

        // Try to find an existing binding for this actor
        FGuid BindingID = FEditingSessionSequencerHelper::FindBindingForObject(LevelSequence, MeshActor);
        if (BindingID.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Reusing existing possessable binding for %s"), *MeshActor->GetName());
        }
        else
        {
            // Fallback: check by label (rare cases)
            for (const FMovieSceneBinding& Binding : static_cast<const UMovieScene*>(MovieScene)->GetBindings())
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
    }

    // Add rig if selected
    AddRigToSequence(LevelSequence, Rig);

    if (IAssetEditorInstance* Inst = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
        ->FindEditorForAsset(LevelSequence, false))
    {
        if (ILevelSequenceEditorToolkit* Toolkit = static_cast<ILevelSequenceEditorToolkit*>(Inst))
        {
            if (TSharedPtr<ISequencer> Seq = Toolkit->GetSequencer())
            {
                Seq->ForceEvaluate(); // ensures ControlRig runtime object is spawned
            }
        }
    }

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
        SetActiveSequence(Existing);
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
    SetActiveSequence(NewSequence);

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

int32 GetSourceFrameCount(const UAnimSequence* Animation)
{
#if WITH_EDITOR
    if (!Animation) return 0;
    if (const IAnimationDataModel* DataModel = Animation->GetDataModel())
    {
        return DataModel->GetNumberOfFrames();
    }
#endif
    return 0;
}

void FEditingSessionSequencerHelper::AddAnimationTrack(ULevelSequence* LevelSequence, UAnimSequence* Animation, FGuid BindingID, bool bSetAnimRange)
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
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Removed old animation track."));
    }

    UMovieSceneSkeletalAnimationTrack* AnimTrack = Cast<UMovieSceneSkeletalAnimationTrack>(
        MovieScene->AddTrack(UMovieSceneSkeletalAnimationTrack::StaticClass(), BindingID)
    );
    if (!AnimTrack) return;

    UMovieSceneSection* Section = AnimTrack->AddNewAnimation(FFrameNumber(0), Animation);
    if (!Section)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] AddNewAnimation returned null. Possibly invalid binding or uninitialized sequence."));
        return;
    }

    if (bSetAnimRange)
    {
        MovieScene->SetPlaybackRangeLocked(false);

        const FFrameRate TickRes = MovieScene->GetTickResolution();
        const FFrameNumber StartTick(0);
        FFrameNumber EndTick = TickRes.AsFrameNumber(Animation->GetPlayLength());
        EndTick = FMath::Max(StartTick + 1, EndTick - 1);

        // no -1 adjustment, and use inclusive range
        MovieScene->SetPlaybackRange(TRange<FFrameNumber>::Inclusive(StartTick, EndTick));
        Section->SetRange(TRange<FFrameNumber>::Inclusive(StartTick, EndTick));
        Section->SetStartFrame(StartTick);

        MovieScene->SetWorkingRange(-0.4f, Animation->GetPlayLength()+0.4f);
        MovieScene->SetViewRange(-0.4f, Animation->GetPlayLength()+0.4f);
    }

    AnimTrack->Modify();
    MovieScene->Modify();

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Added animation '%s' to Level Sequence."), *Animation->GetName());
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
    UControlRig* FoundRig = nullptr;

    if (RigObject->IsA(UControlRig::StaticClass()))
    {
        RigClass = RigObject->GetClass();
        FoundRig = Cast<UControlRig>(RigObject);
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

    if (FoundRig)
    {
        ActiveRig = FoundRig;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return;

    // --- Find the existing SkeletalMeshActor binding in the sequence ---
    FGuid BindingID;
    if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
    {
        for (const FMovieSceneBinding& Binding : static_cast<const UMovieScene*>(MovieScene)->GetBindings())
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

    // --- Create or reuse the ControlRig track ---
    FMovieSceneBindingProxy BindingProxy(BindingID, LevelSequence);
    UMovieSceneTrack* RigTrack = UControlRigSequencerEditorLibrary::FindOrCreateControlRigTrack(
        World, LevelSequence, RigClass, BindingProxy, true);

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

FGuid FEditingSessionSequencerHelper::FindBindingForObject(
    const ULevelSequence* LevelSequence,
    UObject* InObject,
    TSharedPtr<const UE::MovieScene::FSharedPlaybackState> Shared)
{
    if (!LevelSequence || !InObject) return FGuid();

    if (Shared.IsValid())
    {
        return LevelSequence->FindBindingFromObject(InObject, Shared.ToSharedRef());
    }

    const UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return FGuid();

    const UClass* ObjClass = InObject->GetClass();
    const FString ObjLabel = InObject->IsA<AActor>()
        ? static_cast<AActor*>(InObject)->GetActorLabel()
        : InObject->GetName();

    // Use const GetBindings() to avoid C4996
    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        // FindPossessable is non-const in this version, so cast safely
        UMovieScene* NonConstMovieScene = const_cast<UMovieScene*>(MovieScene);
        FMovieScenePossessable* Poss = NonConstMovieScene->FindPossessable(Binding.GetObjectGuid());
        if (!Poss) continue;

        const bool ClassMatch = Poss->GetPossessedObjectClass() == ObjClass;
        const bool NameMatch = Poss->GetName() == ObjLabel;

        if (ClassMatch && NameMatch)
        {
            return Binding.GetObjectGuid();
        }
    }

    return FGuid();
}

void FEditingSessionSequencerHelper::SetActiveSkeletalMeshComponent(USkeletalMeshComponent* InComp)
{
    ActiveSkeletalMeshComponent = InComp;
}

USkeletalMeshComponent* FEditingSessionSequencerHelper::GetActiveSkeletalMeshComponent()
{
    return ActiveSkeletalMeshComponent.Get();
}

void FEditingSessionSequencerHelper::BakeAndSaveAnimation(const FString& AnimName, const FString& SourceAnimPath)
{
#if WITH_EDITOR
    // Active sequence
    ULevelSequence* Sequence = FEditingSessionSequencerHelper::GetActiveSequence();
    if (!Sequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No active sequence found."));
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No editor world found."));
        return;
    }

    // Get the editor Sequencer that is editing this LevelSequence
    UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!EditorSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No AssetEditorSubsystem available."));
        return;
    }

    IAssetEditorInstance* EditorInstance = EditorSubsystem->FindEditorForAsset(Sequence, /*bFocusIfOpen*/false);
    ILevelSequenceEditorToolkit* Toolkit = static_cast<ILevelSequenceEditorToolkit*>(EditorInstance);
    if (!Toolkit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Level Sequence editor is not open for the active sequence."));
        return;
    }

    TSharedPtr<ISequencer> EditorSequencer = Toolkit->GetSequencer();
    if (!EditorSequencer.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to get ISequencer for active sequence."));
        return;
    }

    // Skeletal mesh component currently driven by this sequence
    USkeletalMeshComponent* SkelComp = GetActiveSkeletalMeshComponent();
    if (!SkelComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No active SkeletalMeshComponent found in current sequence."));
        return;
    }

    USkeleton* Skeleton = SkelComp->GetSkeletalMeshAsset()
        ? SkelComp->GetSkeletalMeshAsset()->GetSkeleton()
        : nullptr;

    if (!Skeleton)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Skeletal mesh has no skeleton assigned."));
        return;
    }

    // Make the new anim use the same rate as the sequence (24 fps in your case)
    UAnimationSettings* AnimSettings = UAnimationSettings::Get();
    const FFrameRate OriginalRate = AnimSettings->DefaultFrameRate;
    if (UMovieScene* MovieScene = Sequence->GetMovieScene())
    {
        const FFrameRate SeqRate = MovieScene->GetDisplayRate();

        // temporarily set the default frame rate so we dont get the sampling bug for morphtargets
        AnimSettings->Modify();
        AnimSettings->DefaultFrameRate = SeqRate;
        AnimSettings->SaveConfig();
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Temporarily set AnimSettings DefaultFrameRate to %d/%d"),
            SeqRate.Numerator, SeqRate.Denominator);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[ToucanSequencer] MovieScene not found in sequence."));
    }

    // Create target AnimSequence asset
    const FString Folder = FOutputHelper::EnsureDatedSubfolder();

    FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
    Factory->TargetSkeleton = Skeleton;

    UAnimSequence* NewAnim = Cast<UAnimSequence>(
        AssetTools.Get().CreateAsset(*AnimName, *Folder, UAnimSequence::StaticClass(), Factory)
    );
    if (!NewAnim)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to create AnimSequence asset."));
        return;
    }

    if (!NewAnim)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to create AnimSequence asset."));
        return;
    }

    // Export options: transforms + morph targets
    UAnimSeqExportOption* ExportOptions = NewObject<UAnimSeqExportOption>(GetTransientPackage());
    ExportOptions->bExportTransforms = true;
    ExportOptions->bExportMorphTargets = true;
    ExportOptions->bExportAttributeCurves = true;
    ExportOptions->bTimecodeRateOverride = false;
    ExportOptions->bUseCustomFrameRate = false;
    ExportOptions->bBakeTimecode = false;

    // Use the active editor Sequencer (IMovieScenePlayer) for evaluation
    FAnimExportSequenceParameters Params;
    Params.MovieSceneSequence = Sequence;
    Params.RootMovieSceneSequence = Sequence;

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7)
    Params.bForceUseOfMovieScenePlaybackRange = true;
#endif
    Params.Player = EditorSequencer.Get();    // ISequencer : IMovieScenePlayer

    if (SkelComp->GetSkeletalMeshAsset()->GetMorphTargets().Num() < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[ToucanSequencer] NO morphs found in the skelmeshasset!"));
    }

    FFrameNumber StartFrame = Sequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
    EditorSequencer->SetGlobalTime(StartFrame);
    EditorSequencer->ForceEvaluate();

    SkelComp->TickComponent(0.f, LEVELTICK_All, nullptr);
    SkelComp->RefreshBoneTransforms();
    SkelComp->FinalizeBoneTransform();

    // Bake keys (including morph targets) into the AnimSequence
    if (MovieSceneToolHelpers::ExportToAnimSequence(NewAnim, ExportOptions, Params, SkelComp))
    {
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Baked and saved animation: %s"), *NewAnim->GetPathName());
        FOutputHelper::MarkAssetAsProcessed(SourceAnimPath);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Bake failed for sequence: %s"), *Sequence->GetName());
    }
    
    AnimSettings->DefaultFrameRate = OriginalRate;
    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Restored AnimSettings DefaultFrameRate to %d/%d"),
        OriginalRate.Numerator, OriginalRate.Denominator);
#endif // WITH_EDITOR
}

UControlRig* FEditingSessionSequencerHelper::GetActiveRig()
{
    UControlRig* Rig = ActiveRig.Get();
    if (Rig)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] ActiveRig: %s"), *Rig->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] ActiveRig is null"));
    }
    return Rig;
}

void FEditingSessionSequencerHelper::RemoveRigFromSequence(ULevelSequence* LevelSequence)
{
    if (!LevelSequence)
        return;

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene)
        return;

    TArray<UMovieSceneTrack*> TracksToRemove;

    // Gather all ControlRig parameter tracks
    for (FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        for (UMovieSceneTrack* Track : Binding.GetTracks())
        {
            if (Track && Track->IsA<UMovieSceneControlRigParameterTrack>())
            {
                TracksToRemove.Add(Track);
            }
        }
    }

    for (UMovieSceneTrack* Track : TracksToRemove)
    {
        MovieScene->RemoveTrack(*Track);
    }

    ActiveRig = nullptr;
}

void FEditingSessionSequencerHelper::BakeAndSave() { /* call OnBakeSaveAnimation on current session */ }
void FEditingSessionSequencerHelper::StepFrames(int32 Frames) { /* advance sequencer timeline */ }
void FEditingSessionSequencerHelper::KeyAllControls() {}
void FEditingSessionSequencerHelper::KeyZeroAll() {}
