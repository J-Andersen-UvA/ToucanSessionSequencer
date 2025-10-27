#include "SEditingSessionWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Misc/ConfigCacheIni.h"
#include "EditingSessionSequencerHelper.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/MessageDialog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "QueueControls.h"

void SEditingSessionWindow::Construct(const FArguments&)
{
    RefreshQueue();
    FSeqQueue::Get().OnQueueChanged().AddSP(this, &SEditingSessionWindow::RefreshQueue);

    ChildSlot
        [
            SNew(SBorder).Padding(8)
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildSelectionRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildStatusRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(4.0f)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildQueueAdditionControlsRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildQueueRemovalControlsRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(4.0f)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildSessionControlsRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(4.0f)]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 8)
                        [
                            SNew(SBorder)
                                .Padding(8)
                                [
                                    SNew(SVerticalBox)
                                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("Queue:")))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                        ]
                                        + SVerticalBox::Slot().FillHeight(1.f)
                                        [
                                            BuildQueueList()
                                        ]
                                ]
                        ]
                ]
        ];

    LoadSettings();
}

TSharedRef<SWidget> SEditingSessionWindow::BuildSelectionRow()
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Pick:")))
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("Skeletal Mesh")))
                .OnClicked(this, &SEditingSessionWindow::OnSelectSkeletalMesh)
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("Control Rig")))
                .OnClicked(this, &SEditingSessionWindow::OnSelectRig)
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("Output Folder")))
                .OnClicked(this, &SEditingSessionWindow::OnSelectOutputFolder)
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildStatusRow()
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Mesh:")))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 16, 0)
        [
            SNew(STextBlock)
                .Text_Lambda([this]() {
                return FText::FromString(SelectedMesh.IsValid() ? SelectedMesh->GetName() : TEXT("None"));
                    })
                .ColorAndOpacity_Lambda([this]() {
                return SelectedMesh.IsValid() ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Red);
                    })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Rig:")))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 16, 0)
        [
            SNew(STextBlock)
                .Text_Lambda([this]() {
                if (SelectedRig.IsNull())
                    return FText::FromString(TEXT("None"));
                return FText::FromString(
                    SelectedRig.IsValid()
                    ? SelectedRig->GetName()
                    : SelectedRig.ToSoftObjectPath().GetAssetName());
                    })
                .ColorAndOpacity_Lambda([this]() {
                return SelectedRig.IsNull() ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground();
                    })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Output Folder:")))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text_Lambda([this]() { return FText::FromString(FOutputHelper::Get()); })
                .ColorAndOpacity_Lambda([this]() {
                return FOutputHelper::Get().IsEmpty() ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground();
                    })
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildQueueAdditionControlsRow()
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Queue animations from: ")))
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("folder")))
                .OnClicked_Lambda([]{ FQueueControls::AddAnimationsFromFolder(); return FReply::Handled(); })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("anim sequence(s)")))
                .OnClicked_Lambda([]{ FQueueControls::AddAnimationsByHand(); return FReply::Handled(); })
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildQueueRemovalControlsRow()
{
    return SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Remove queued animations: ")))
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("all animations")))
                .OnClicked_Lambda([] { FQueueControls::RemoveAllAnimations(); return FReply::Handled(); })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("marked processed")))
                .OnClicked_Lambda([] { FQueueControls::RemoveMarkedProcessedAnimations(); return FReply::Handled(); })
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildSessionControlsRow()
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Session Controls:")))
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Bake & Save Animation")))
            .OnClicked(this, &SEditingSessionWindow::OnBakeSaveAnimation)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
        SNew(SButton)
        .Text(FText::FromString(TEXT("Load Next Animation")))
        .OnClicked(this, &SEditingSessionWindow::OnLoadNextAnimation)
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildQueueList()
{
    return SAssignNew(ListView, SListView<TSharedPtr<FQueuedAnim>>)
        .ListItemsSource(&Rows)
        .OnGenerateRow(this, &SEditingSessionWindow::OnMakeRow)
        .SelectionMode(ESelectionMode::None);
}

void SEditingSessionWindow::LoadSettings()
{
#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif

    FString MeshPath, RigPath, FolderPath;
    GConfig->GetString(CfgSection, MeshKey, MeshPath, Ini);
    GConfig->GetString(CfgSection, RigKey, RigPath, Ini);

    if (!MeshPath.IsEmpty())
        SelectedMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));

    if (!RigPath.IsEmpty())
        SelectedRig = TSoftObjectPtr<UObject>(FSoftObjectPath(RigPath));
}

void SEditingSessionWindow::SaveSettings() const
{
#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif

    if (SelectedMesh.IsValid())
        GConfig->SetString(CfgSection, MeshKey, *SelectedMesh.ToSoftObjectPath().ToString(), Ini);

    const FSoftObjectPath RigPath = SelectedRig.ToSoftObjectPath();
    if (RigPath.IsValid())
        GConfig->SetString(CfgSection, RigKey, *RigPath.ToString(), Ini);

    GConfig->Flush(false, Ini);
}

void SEditingSessionWindow::RefreshQueue()
{
    Rows.Reset();
    const auto& All = FSeqQueue::Get().GetAll();
    for (const auto& Q : All)
        Rows.Add(MakeShared<FQueuedAnim>(Q));

    if (ListView.IsValid())
        ListView->RequestListRefresh();
}

TSharedRef<ITableRow> SEditingSessionWindow::OnMakeRow(
    TSharedPtr<FQueuedAnim> Item, const TSharedRef<STableViewBase>& Owner)
{
    return SNew(STableRow<TSharedPtr<FQueuedAnim>>, Owner)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    // Load this specific animation
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("->")))
                        .IsEnabled_Lambda([this, Item]() {
                        int32 RowIndex = Rows.IndexOfByKey(Item);
                        return RowIndex != CurrentIndex; // disable for current
                            })
                        .OnClicked_Lambda([this, Item]() {
                        int32 RowIndex = Rows.IndexOfByKey(Item);
                        if (RowIndex != INDEX_NONE)
                        {
                            CurrentIndex = RowIndex;
                            LoadAnimationAtIndex(RowIndex);
                            RefreshQueue();
                        }
                        return FReply::Handled();
                            })
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                .Padding(8.f, 0.f, 0.f, 0.f)
                [
                    SNew(STextBlock)
                        .Text_Lambda([this, Item]() {
                        if (!Item.IsValid())
                            return FText::GetEmpty();

                        int32 RowIndex = Rows.IndexOfByKey(Item);
                        FString Label = Item->DisplayName.ToString();

                        UObject* Asset = UEditorAssetLibrary::LoadAsset(Item->Path.ToString());
                        if (Asset)
                        {
                            const FString TagValue = UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Processed"));
                            if (TagValue.Equals(TEXT("True"), ESearchCase::IgnoreCase))
                                Label += TEXT(" (Already processed?)");
                        }

                        if (RowIndex == CurrentIndex)
                            Label += TEXT("  <-- editing");

                        return FText::FromString(Label);
                            })
                        .ColorAndOpacity_Lambda([this, Item]() {
                        if (!Item.IsValid())
                            return FLinearColor::White;

                        int32 RowIndex = Rows.IndexOfByKey(Item);
                        if (RowIndex == CurrentIndex)
                            return FLinearColor(0.2f, 0.4f, 1.0f); // blue highlight

                        UObject* Asset = UEditorAssetLibrary::LoadAsset(Item->Path.ToString());
                        if (Asset)
                        {
                            const FString TagValue = UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Processed"));
                            if (TagValue.Equals(TEXT("True"), ESearchCase::IgnoreCase))
                                return FLinearColor::Red;
                        }

                        return FLinearColor::White;
                            })
                ]

            // --- "Unmark processed" button, only visible if processed ---
            + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Unmark processed")))
                        .Visibility_Lambda([Item]() {
                        if (!Item.IsValid())
                            return EVisibility::Collapsed;
                        UObject* Asset = UEditorAssetLibrary::LoadAsset(Item->Path.ToString());
                        if (!Asset)
                            return EVisibility::Collapsed;
                        return UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Processed")) == TEXT("True")
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                            })
                        .OnClicked_Lambda([this, Item]() {
                        if (Item.IsValid())
                        {
                            UObject* Asset = UEditorAssetLibrary::LoadAsset(Item->Path.ToString());
                            if (Asset)
                            {
                                UEditorAssetLibrary::SetMetadataTag(Asset, TEXT("Processed"), TEXT("False"));
                                UEditorAssetLibrary::SaveLoadedAsset(Asset);
                                RefreshQueue();
                            }
                        }
                        return FReply::Handled();
                            })
                ]

            // --- "Remove" button ---
            + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Remove")))
                        .OnClicked_Lambda([Item]() {
                        const auto& All = FSeqQueue::Get().GetAll();
                        if (!Item.IsValid())
                            return FReply::Handled();

                        int32 Index = INDEX_NONE;
                        for (int32 i = 0; i < All.Num(); ++i)
                        {
                            if (All[i].Path == Item->Path)
                            {
                                Index = i;
                                break;
                            }
                        }

                        if (Index != INDEX_NONE)
                            FSeqQueue::Get().RemoveAt(Index);

                        return FReply::Handled();
                            })
                ]
        ];
}

FReply SEditingSessionWindow::OnSelectSkeletalMesh()
{
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

    FOpenAssetDialogConfig Config;
    Config.DialogTitleOverride = FText::FromString(TEXT("Select Skeletal Mesh"));
    Config.AssetClassNames.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    Config.bAllowMultipleSelection = false;

    CB.Get().CreateOpenAssetDialog(
        Config,
        FOnAssetsChosenForOpen::CreateLambda([this](const TArray<FAssetData>& Selected)
        {
            if (Selected.Num() > 0)
            {
                SelectedMesh = Cast<USkeletalMesh>(Selected[0].GetAsset());
                SaveSettings();
            }

        }),
        FOnAssetDialogCancelled::CreateLambda([] {})
    );

    return FReply::Handled();
}

FReply SEditingSessionWindow::OnSelectRig()
{
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

    FOpenAssetDialogConfig Config;
    Config.DialogTitleOverride = FText::FromString(TEXT("Select Control Rig"));

    Config.AssetClassNames.Add(FTopLevelAssetPath(TEXT("/Script/ControlRig"), TEXT("ControlRigBlueprint")));
    Config.AssetClassNames.Add(FTopLevelAssetPath(TEXT("/Script/ControlRigDeveloper"), TEXT("ControlRigBlueprint")));

    Config.bAllowMultipleSelection = false;

    CB.Get().CreateOpenAssetDialog(
        Config,
        FOnAssetsChosenForOpen::CreateLambda([this](const TArray<FAssetData>& Selected)
            {
                if (Selected.Num() > 0)
                {
                    SelectedRig = Selected[0].GetAsset(); // store the blueprint asset
                    SaveSettings();
                }
            }),
        FOnAssetDialogCancelled::CreateLambda([] {})
    );

    return FReply::Handled();
}

FReply SEditingSessionWindow::OnSelectOutputFolder()
{
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    IContentBrowserSingleton& CBSingleton = CB.Get();

    FPathPickerConfig PathPickerConfig;
    PathPickerConfig.DefaultPath = FOutputHelper::Get();
    PathPickerConfig.bAllowContextMenu = true;
    PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([](const FString& SelectedPath)
        {
            FOutputHelper::Set(SelectedPath);
        });

    TSharedRef<SWindow> PickerWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Select Output Folder")))
        .ClientSize(FVector2D(400, 300))
        .SupportsMinimize(false)
        .SupportsMaximize(false)
        [
            CBSingleton.CreatePathPicker(PathPickerConfig)
        ];

    FSlateApplication::Get().AddWindow(PickerWindow);
    return FReply::Handled();
}

FReply SEditingSessionWindow::OnBakeSaveAnimation()
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(CurrentIndex))
        return FReply::Handled();

    const FQueuedAnim& CurrentAnim = All[CurrentIndex];
    FString SourceAnimPath = CurrentAnim.Path.ToString();

    FString AnimName = FPaths::GetBaseFilename(SourceAnimPath);

    FEditingSessionSequencerHelper::BakeAndSaveAnimation(AnimName, SourceAnimPath);
    return FReply::Handled();
}

FReply SEditingSessionWindow::OnLoadNextAnimation()
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (All.Num() == 0)
        return FReply::Handled();

    // Determine next index
    if (CurrentIndex == INDEX_NONE)
        CurrentIndex = 0;
    else
        CurrentIndex = (CurrentIndex + 1) % All.Num();

    if (ListView.IsValid())
        ListView->RebuildList();

    // Load animation asset
    UObject* AnimObject = All[CurrentIndex].Path.TryLoad();
    if (UEditorAssetLibrary::GetMetadataTag(AnimObject, TEXT("Processed")) == TEXT("True"))
    {
        const FString AnimName = AnimObject->GetName();
        const FString AnimPath = All[CurrentIndex].Path.ToString();

        const FText Title = FText::FromString(TEXT("Processed Animation Detected"));
        
        const FText Message = FText::Format(
            FText::FromString(TEXT("Animation:\t'{0}'\nPath:\t'{1}'\nNote:\tis marked as 'Processed'.\nAction:\tdo you want to load it anyway?")),
            FText::FromString(AnimName),
            FText::FromString(AnimPath)
        );

        EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);
        if (Response == EAppReturnType::No)
        {
            UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Skipped processed animation: %s"), *AnimPath);
            return FReply::Handled();
        }
    }

    UAnimSequence* Anim = Cast<UAnimSequence>(AnimObject);
    if (!Anim)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to load animation asset."));
        return FReply::Handled();
    }

    UObject* RigObj = SelectedRig.LoadSynchronous(); // or TryLoad()

    // Delegate to helper
    FEditingSessionSequencerHelper::LoadNextAnimation(SelectedMesh, RigObj, Anim);

    return FReply::Handled();
}

void SEditingSessionWindow::LoadAnimationAtIndex(int32 TargetIndex)
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(TargetIndex))
        return;

    CurrentIndex = TargetIndex;

    if (ListView.IsValid())
        ListView->RebuildList();

    UObject* AnimObject = All[CurrentIndex].Path.TryLoad();
    if (!AnimObject)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to load animation at index %d."), TargetIndex);
        return;
    }

    // Check for "Processed" metadata
    if (UEditorAssetLibrary::GetMetadataTag(AnimObject, TEXT("Processed")) == TEXT("True"))
    {
        const FString AnimName = AnimObject->GetName();
        const FString AnimPath = All[CurrentIndex].Path.ToString();

        const FText Title = FText::FromString(TEXT("Processed Animation Detected"));
        const FText Message = FText::Format(
            FText::FromString(TEXT("Animation:\t'{0}'\nPath:\t'{1}'\nNote:\tis marked as 'Processed'.\nAction:\tdo you want to load it anyway?")),
            FText::FromString(AnimName),
            FText::FromString(AnimPath)
        );

        const EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);
        if (Response == EAppReturnType::No)
        {
            UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Skipped processed animation: %s"), *AnimPath);
            return;
        }
    }

    UAnimSequence* Anim = Cast<UAnimSequence>(AnimObject);
    if (!Anim)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Invalid animation type."));
        return;
    }

    UObject* RigObj = SelectedRig.LoadSynchronous();

    FEditingSessionSequencerHelper::LoadNextAnimation(SelectedMesh, RigObj, Anim);

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Loaded animation at index %d: %s"), TargetIndex, *Anim->GetName());
}
