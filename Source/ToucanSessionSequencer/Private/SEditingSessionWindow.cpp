#include "SEditingSessionWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/StyleColors.h"
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
#include "Misc/CoreDelegates.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetExportTask.h"
#include "Exporters/Exporter.h"
#include "Exporters/AnimSequenceExporterFBX.h"
#include "Exporters/FbxExportOption.h"
#include "Animation/AnimSequence.h"
#include "EditingSessionDelegates.h"

TSharedRef<SWidget> SEditingSessionWindow::AddIconHere(const FString& IconName, const FVector2D& Size)
{
    const FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ToucanSessionSequencer"))->GetBaseDir() / TEXT("Resources/Icons");
    const FString IconPath = ContentDir / IconName + TEXT(".svg");

    const FVector2D IconSize(18.f, 18.f);
    return SNew(SImage)
        .Image(new FSlateVectorImageBrush(IconPath, IconSize, FLinearColor::White))
        .ColorAndOpacity(FLinearColor::White);
}

TSharedRef<SWidget> SEditingSessionWindow::AddIconAndTextHere(
    const FString& IconName,
    const FString& Text,
    bool bBold /*= false*/,
    bool bUseAppStyleIcon /*= false*/,
    const FLinearColor& IconColor /*= FLinearColor(0,0,0,0)*/
)
{
    const FVector2D IconSize(16.f, 16.f);
    const FSlateBrush* IconBrush = nullptr;

    if (bUseAppStyleIcon)
    {
        IconBrush = FAppStyle::GetBrush(*IconName);
    }
    else
    {
        IconBrush = new FSlateVectorImageBrush(
            IPluginManager::Get().FindPlugin(TEXT("ToucanSessionSequencer"))->GetBaseDir()
            / TEXT("Resources/Icons") / (IconName + TEXT(".svg")),
            IconSize,
            IconColor
        );
    }

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
        [
            SNew(SImage)
                .Image(IconBrush)
                .ColorAndOpacity(
                    (IconColor.R == 0.f && IconColor.G == 0.f && IconColor.B == 0.f && IconColor.A == 0.f)
                    ? FSlateColor::UseForeground()
                    : FSlateColor(IconColor)
                )
                .DesiredSizeOverride(IconSize)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Text))
                .Font(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", 10))
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::AddIconAndTextHere(const FString& IconName, const FString& Text, bool bBold, bool bUseAppStyleIcon)
{
    const FVector2D IconSize(16.f, 16.f);
    const FSlateBrush* IconBrush = nullptr;

    if (bUseAppStyleIcon)
    {
        IconBrush = FAppStyle::GetBrush(*IconName);
    }
    else
    {
        IconBrush = new FSlateVectorImageBrush(
            IPluginManager::Get().FindPlugin(TEXT("ToucanSessionSequencer"))->GetBaseDir()
            / TEXT("Resources/Icons") / (IconName + TEXT(".svg")),
            IconSize,
            FLinearColor::White);
    }

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
        [
            SNew(SImage)
                .Image(IconBrush)
                .ColorAndOpacity(FLinearColor::White)
                .DesiredSizeOverride(IconSize)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Text))
                .Font(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", 10))
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::AddIconAndTextHere(const FString& IconName, const FString& Text, const bool bBold)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
        [
            AddIconHere(IconName)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Text))
                .Font(bBold
                    ? FCoreStyle::GetDefaultFontStyle("Bold", 10)
                    : FCoreStyle::GetDefaultFontStyle("Regular", 10))
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::AddIconAndTextHere(const FString& IconName, const FString& Text)
{
    return AddIconAndTextHere(IconName, Text, false);
}

namespace
{
    bool IsQueuedAnimProcessed(const FQueuedAnim& Item)
    {
        return Item.bProcessed;
    }

    bool TryGetCheckpointPath(const FQueuedAnim& Item, FString& OutCheckpointPath)
    {
        OutCheckpointPath.Reset();
        if (!Item.bCheckpointed || Item.CheckpointPath.IsEmpty())
            return false;

        OutCheckpointPath = Item.CheckpointPath;
        return UEditorAssetLibrary::DoesAssetExist(OutCheckpointPath);
    }

    bool IsQueuedAnimCheckpointed(const FQueuedAnim& Item)
    {
        FString CheckpointPath;
        return TryGetCheckpointPath(Item, CheckpointPath);
    }

    void SyncQueueStatusFromLoadedAsset(const FQueuedAnim& Item, UObject* Asset)
    {
        if (!Asset)
            return;

        FSeqQueue& Queue = FSeqQueue::Get();

        const FString Checkpointed = UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Checkpointed"));
        const FString CheckpointPath = UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("CheckpointPath"));
        if (Checkpointed.Equals(TEXT("True"), ESearchCase::IgnoreCase) &&
            !CheckpointPath.IsEmpty() &&
            UEditorAssetLibrary::DoesAssetExist(CheckpointPath))
        {
            Queue.SetProcessed(Item.Path, false);
            Queue.SetCheckpoint(Item.Path, CheckpointPath);
            return;
        }

        Queue.ClearCheckpoint(Item.Path);

        const FString Processed = UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Processed"));
        if (Processed.Equals(TEXT("True"), ESearchCase::IgnoreCase))
        {
            Queue.SetProcessed(Item.Path, true);
        }
        else
        {
            Queue.SetProcessed(Item.Path, false);
        }
    }

    FString NormalizeMatchString(const FString& InString)
    {
        FString Out;
        Out.Reserve(InString.Len());
        for (TCHAR Char : InString)
        {
            if (FChar::IsAlnum(Char))
            {
                Out.AppendChar(FChar::ToLower(Char));
            }
        }
        return Out;
    }

    int32 LongestCommonSubsequenceLength(const FString& A, const FString& B)
    {
        if (A.IsEmpty() || B.IsEmpty())
        {
            return 0;
        }

        TArray<int32> Previous;
        TArray<int32> Current;
        Previous.Init(0, B.Len() + 1);
        Current.Init(0, B.Len() + 1);

        for (int32 AIndex = 1; AIndex <= A.Len(); ++AIndex)
        {
            for (int32 BIndex = 1; BIndex <= B.Len(); ++BIndex)
            {
                if (A[AIndex - 1] == B[BIndex - 1])
                {
                    Current[BIndex] = Previous[BIndex - 1] + 1;
                }
                else
                {
                    Current[BIndex] = FMath::Max(Previous[BIndex], Current[BIndex - 1]);
                }
            }
            Swap(Previous, Current);
            Current.Init(0, B.Len() + 1);
        }

        return Previous[B.Len()];
    }

    int32 ScoreVideoNameMatch(const FString& QueueName, const FString& VideoName)
    {
        const FString NormalizedQueueName = NormalizeMatchString(QueueName);
        const FString NormalizedVideoName = NormalizeMatchString(VideoName);
        if (NormalizedQueueName.IsEmpty() || NormalizedVideoName.IsEmpty())
        {
            return 0;
        }

        if (NormalizedVideoName.Contains(NormalizedQueueName) || NormalizedQueueName.Contains(NormalizedVideoName))
        {
            return 1000 + FMath::Min(NormalizedQueueName.Len(), NormalizedVideoName.Len());
        }

        const int32 LcsLength = LongestCommonSubsequenceLength(NormalizedQueueName, NormalizedVideoName);
        const int32 Denominator = FMath::Max(NormalizedQueueName.Len(), NormalizedVideoName.Len());
        return Denominator > 0 ? FMath::RoundToInt(100.0f * static_cast<float>(LcsLength) / static_cast<float>(Denominator)) : 0;
    }
}

void SEditingSessionWindow::Construct(const FArguments&)
{
    RefreshQueue();
    FSeqQueue::Get().OnQueueChanged().AddSP(this, &SEditingSessionWindow::RefreshQueue);

    ChildSlot
        [
            SNew(SBorder).Padding(8)
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[BuildSelectionAndStatusGrid()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[BuildQueueControlsSection()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[BuildSessionControlsRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[BuildProgressBarSection()]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 8)
                        [
                            SNew(SBorder)
                                .Padding(FMargin(8, 6))
                                [
                                    SNew(SVerticalBox)
                                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                                        [
                                            AddIconAndTextHere(TEXT("queueClipboardWhite"), TEXT("Queue:"), true)
                                        ]
                                        + SVerticalBox::Slot().FillHeight(1.f).MaxHeight(400.f)
                                        [
                                            BuildQueueList()
                                        ]
                                ]
                        ]
                ]
        ];

    LoadSettings();
}

TSharedRef<SWidget> SEditingSessionWindow::BuildSelectionAndStatusGrid()
{
    return SNew(SBorder)
        .Padding(FMargin(8, 6))
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [
            SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 6)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Configuration")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(4.0f)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [
                    SNew(SGridPanel)
                        // Row 0: skeletalmesh
                        + SGridPanel::Slot(0, 0).Padding(0, 0, 4, 2).VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                                .OnClicked(this, &SEditingSessionWindow::OnSelectSkeletalMesh)
                                [
                                    AddIconAndTextHere(TEXT("ClassIcon.SkeletalMesh"), TEXT("Skeletal Mesh"), false, true)
                                ]
                        ]
                    + SGridPanel::Slot(1, 0).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]() {
                                return FText::FromString(SelectedMesh.IsValid() ? SelectedMesh->GetName() : TEXT("None"));
                                    })
                                .ColorAndOpacity_Lambda([this]() {
                                return SelectedMesh.IsValid() ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Red);
                                    })
                        ]

                    // Row 1: Control Rig
                    + SGridPanel::Slot(0, 1).Padding(0, 2, 4, 2)
                        [
                            SNew(SButton)
                                .OnClicked(this, &SEditingSessionWindow::OnSelectRig)
                                [
                                    AddIconAndTextHere(TEXT("controlRigWhite"), TEXT("Rig"))
                                ]
                        ]
                    + SGridPanel::Slot(1, 1).VAlign(VAlign_Center)
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

                    // Row 2 � Output Folder
                    + SGridPanel::Slot(0, 2).Padding(0, 2, 4, 0)
                        [
                            SNew(SButton)
                                .OnClicked(this, &SEditingSessionWindow::OnSelectOutputFolder)
                                [
                                    AddIconAndTextHere(TEXT("Icons.FolderOpen"), TEXT("Output"), false, true)
                                ]
                        ]
                    + SGridPanel::Slot(1, 2).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]() { return FText::FromString(FOutputHelper::Get()); })
                                .ColorAndOpacity_Lambda([this]() {
                                return FOutputHelper::Get().IsEmpty() ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground();
                                    })
                        ]

                    + SGridPanel::Slot(0, 3).Padding(0, 2, 4, 0)
                        [
                            SNew(SButton)
                                .OnClicked(this, &SEditingSessionWindow::OnSelectVideoFolder)
                                [
                                    AddIconAndTextHere(TEXT("Icons.FolderOpen"), TEXT("Videos"), false, true)
                                ]
                        ]
                    + SGridPanel::Slot(1, 3).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]() {
                                    return FText::FromString(SelectedVideoFolder.IsEmpty() ? TEXT("None") : SelectedVideoFolder);
                                })
                                .ColorAndOpacity_Lambda([this]() {
                                    return SelectedVideoFolder.IsEmpty() ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground();
                                })
                        ]
                    ]
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildQueueControlsSection()
{
    return SNew(SBorder)
        .Padding(FMargin(8, 6))
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [
            SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 6)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Queue Management")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(4.0f)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [
                    BuildQueueAdditionControlsRow()
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    BuildQueueRemovalControlsRow()
                ]
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildQueueAdditionControlsRow()
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Add:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
        [
            SNew(SButton)
                .OnClicked_Lambda([]{ FQueueControls::AddAnimationsFromFolder(); return FReply::Handled(); })
                [
                    AddIconAndTextHere(TEXT("Icons.Plus"), TEXT("folder"), false, true, FStyleColors::AccentGreen.GetSpecifiedColor())
                ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
        [
            SNew(SButton)
                .OnClicked_Lambda([] { FQueueControls::AddAnimationsByHand(); return FReply::Handled(); })
                [
                    AddIconAndTextHere(TEXT("Icons.Plus"), TEXT("anim sequence(s)"), false, true, FStyleColors::AccentGreen.GetSpecifiedColor())
                ]
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildQueueRemovalControlsRow()
{
    return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
            [
                //AddIconAndTextHere(TEXT("minusWhite"), TEXT("Remove:"))
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Rem:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
            [
                SNew(SButton)
                    .OnClicked_Lambda([] { FQueueControls::RemoveAllAnimations(); return FReply::Handled(); })
                    [
                        AddIconAndTextHere(TEXT("Icons.Minus"), TEXT("all"), false, true, FStyleColors::AccentRed.GetSpecifiedColor())
                    ]
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
            [
                SNew(SButton)
                    .OnClicked_Lambda([] { FQueueControls::RemoveMarkedProcessedAnimations(); return FReply::Handled(); })
                    [
                        AddIconAndTextHere(TEXT("Icons.Minus"), TEXT("processed"), false, true, FStyleColors::AccentRed.GetSpecifiedColor())
                    ]
            ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildSessionControlsRow()
{
    return SNew(SBorder)
        .Padding(FMargin(8, 6))
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [
            SNew(SVerticalBox)

                // Title row
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Session Actions")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(4.0f)]
                // Buttons row
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                                [
                                    SNew(SButton)
                                        //.Text(FText::FromString(TEXT("Bake & Save")))
                                        .OnClicked(this, &SEditingSessionWindow::OnBakeSaveAnimation)
                                        [
                                            AddIconAndTextHere(TEXT("Icons.Save"), TEXT("Bake & Save"), false, true)
                                        ]
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                                [
                                    SNew(SButton)
                                        .OnClicked(this, &SEditingSessionWindow::OnCheckpointCurrentAnimation)
                                        [
                                            AddIconAndTextHere(TEXT("Sequencer.Tracks.Event"), TEXT("Checkpoint"), false, true)
                                        ]
                                ]
                                + SHorizontalBox::Slot().FillWidth(1.f)
                                [
                                    SNew(SSpacer)
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                                [
                                    SNew(SButton)
                                        //.Text(FText::FromString(TEXT("Next Animation")))
                                        .OnClicked(this, &SEditingSessionWindow::OnLoadNextAnimation)
                                        [
                                            AddIconAndTextHere(TEXT("Icons.ChevronRight"), TEXT("Next Todo"), false, true)
                                        ]
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [
                                    SNew(SButton)
                                        .OnClicked(this, &SEditingSessionWindow::OnBakeSaveAnimationTo)
                                        [
                                            AddIconAndTextHere(TEXT("Icons.Save"), TEXT("Bake & Save To"), false, true)
                                        ]
                                ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                                [
                                    SNew(SButton)
                                        .OnClicked(this, &SEditingSessionWindow::OnLoadVideoForCurrent)
                                        [
                                            AddIconAndTextHere(TEXT("Icons.FolderOpen"), TEXT("Load video for current"), false, true)
                                        ]
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                                [
                                    SNew(SButton)
                                        .OnClicked(this, &SEditingSessionWindow::OnExportFolder)
                                        [
                                            AddIconAndTextHere(TEXT("Icons.FolderOpen"), TEXT("Export Anims in Folder To"), false, true)
                                        ]
                                ]
                        ]
                ]
        ];
}

TSharedRef<SWidget> SEditingSessionWindow::BuildProgressBarSection()
{
    return SNew(SBorder)
        .Padding(FMargin(8, 6))
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Session Progress")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            ]

            + SVerticalBox::Slot()
            .AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(SSeparator).Thickness(4.0f)
            ]
            + SVerticalBox::Slot()
            .AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                    .Text_Lambda([]() -> FText
                        {
                            const int32 Total = FSeqQueue::Get().GetAll().Num();
                            const int32 Processed = FSeqQueue::Get().GetProcessedCount();
                            const int32 Index = FSeqQueue::Get().GetCurrentIndex();

                            return FText::Format(
                                NSLOCTEXT("ToucanSessionSequencer", "SessHdr",
                                    "processed: {0}/{1}  idx:{2}"),
                                FText::AsNumber(Processed),
                                FText::AsNumber(Total),
                                FText::AsNumber(Index + 1)
                            );
                        })
            ]
            + SVerticalBox::Slot()
            .AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 4)
            [
                SNew(SBox)
                    .WidthOverride(220.f)
                    .HeightOverride(8.f)
                    [
                        SNew(SProgressBar)
                            .Percent(
                                TAttribute<TOptional<float>>::Create(
                                    TAttribute<TOptional<float>>::FGetter::CreateLambda([]() -> TOptional<float>
                                        {
                                            const int32 Total = FSeqQueue::Get().GetAll().Num();
                                            const int32 Processed = FSeqQueue::Get().GetProcessedCount();
                                            if (Total <= 0) { return TOptional<float>(); }
                                            return float(FMath::Clamp(Processed, 0, Total)) / float(Total);
                                        })
                                )
                            )
                    ]
            ]
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

    FString MeshPath, RigPath, VideoFolderPath, BakeSaveToFolderPath;
    GConfig->GetString(CfgSection, MeshKey, MeshPath, Ini);
    GConfig->GetString(CfgSection, RigKey, RigPath, Ini);
    GConfig->GetString(CfgSection, VideoFolderKey, VideoFolderPath, Ini);
    GConfig->GetString(CfgSection, BakeSaveToFolderKey, BakeSaveToFolderPath, Ini);

    if (!MeshPath.IsEmpty())
        SelectedMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));

    if (!RigPath.IsEmpty())
        SelectedRig = TSoftObjectPtr<UObject>(FSoftObjectPath(RigPath));

    if (!VideoFolderPath.IsEmpty())
        SelectedVideoFolder = VideoFolderPath;

    BakeSaveToFolder = BakeSaveToFolderPath.IsEmpty() ? FOutputHelper::Get() : BakeSaveToFolderPath;
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

    GConfig->SetString(CfgSection, VideoFolderKey, *SelectedVideoFolder, Ini);
    GConfig->SetString(CfgSection, BakeSaveToFolderKey, *BakeSaveToFolder, Ini);
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
                        return RowIndex != FSeqQueue::Get().GetCurrentIndex(); // disable for current
                            })
                        .OnClicked_Lambda([this, Item]() {
                        int32 RowIndex = Rows.IndexOfByKey(Item);
                        if (RowIndex != INDEX_NONE)
                        {
                            FSeqQueue::Get().SetCurrentIndex(RowIndex);
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

                        if (IsQueuedAnimProcessed(*Item))
                            Label += TEXT(" (Already processed?)");

                        FString CheckpointPath;
                        if (TryGetCheckpointPath(*Item, CheckpointPath))
                            Label += TEXT(" (Checkpointed)");

                        if (RowIndex == FSeqQueue::Get().GetCurrentIndex())
                            Label += TEXT("  <-- editing");

                        return FText::FromString(Label);
                            })
                        .ColorAndOpacity_Lambda([this, Item]() {
                        if (!Item.IsValid())
                            return FLinearColor::White;

                        int32 RowIndex = Rows.IndexOfByKey(Item);
                        if (RowIndex == FSeqQueue::Get().GetCurrentIndex())
                            return FLinearColor(0.2f, 0.4f, 1.0f); // blue highlight

                        if (IsQueuedAnimProcessed(*Item))
                            return FLinearColor::Red;

                        FString CheckpointPath;
                        if (TryGetCheckpointPath(*Item, CheckpointPath))
                            return FLinearColor::Yellow;

                        return FLinearColor::White;
                            })
                ]

            // --- "Continue from checkpoint" button, only visible if checkpointed ---
            + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.f, 0.f, 0.f, 0.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Continue from checkpoint")))
                        .Visibility_Lambda([Item]() {
                        if (!Item.IsValid())
                            return EVisibility::Collapsed;
                        return IsQueuedAnimCheckpointed(*Item)
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                            })
                        .OnClicked_Lambda([this, Item]() {
                        if (Item.IsValid())
                        {
                            int32 RowIndex = Rows.IndexOfByKey(Item);
                            if (RowIndex != INDEX_NONE)
                            {
                                ContinueFromCheckpointAtIndex(RowIndex);
                                RefreshQueue();
                            }
                        }
                        return FReply::Handled();
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
                        return IsQueuedAnimProcessed(*Item)
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
                                FSeqQueue::Get().SetProcessed(Item->Path, false);
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

                    FString RigName = FPaths::GetCleanFilename(SelectedRig->GetName());
                    GOnRigChanged.Broadcast(RigName);

                    UE_LOG(LogTemp, Log, TEXT("Rig changed: %s"), *RigName);
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

FReply SEditingSessionWindow::OnSelectVideoFolder()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] DesktopPlatform unavailable; cannot open video folder picker."));
        return FReply::Handled();
    }

    FString SelectedFolder;
    const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    const bool bSelected = DesktopPlatform->OpenDirectoryDialog(
        ParentWindowHandle,
        TEXT("Select Video Folder"),
        SelectedVideoFolder.IsEmpty() ? FPaths::ProjectDir() : SelectedVideoFolder,
        SelectedFolder
    );

    if (bSelected && !SelectedFolder.IsEmpty())
    {
        SelectedVideoFolder = SelectedFolder;
        SaveSettings();
        LoadBestMatchedVideoForCurrent();
    }

    return FReply::Handled();
}

FReply SEditingSessionWindow::OnBakeSaveAnimation()
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(FSeqQueue::Get().GetCurrentIndex()))
        return FReply::Handled();

    const FQueuedAnim& CurrentAnim = All[FSeqQueue::Get().GetCurrentIndex()];
    FString SourceAnimPath = CurrentAnim.Path.ToString();

    FString AnimName = FPaths::GetBaseFilename(SourceAnimPath);

    FEditingSessionSequencerHelper::BakeAndSaveAnimation(AnimName, SourceAnimPath);
    return FReply::Handled();
}

FReply SEditingSessionWindow::OnBakeSaveAnimationTo()
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(FSeqQueue::Get().GetCurrentIndex()))
        return FReply::Handled();

    const FQueuedAnim CurrentAnim = All[FSeqQueue::Get().GetCurrentIndex()];
    const FString SourceAnimPath = CurrentAnim.Path.ToString();
    const FString AnimName = FPaths::GetBaseFilename(SourceAnimPath);

    const FString DefaultBakeFolder = BakeSaveToFolder.IsEmpty() ? FOutputHelper::Get() : BakeSaveToFolder;
    TSharedPtr<FString> PickedContentFolder = MakeShared<FString>(DefaultBakeFolder);

    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

    FPathPickerConfig PathPickerConfig;
    PathPickerConfig.DefaultPath = DefaultBakeFolder;
    PathPickerConfig.bAllowContextMenu = true;
    PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([PickedContentFolder](const FString& SelectedPath)
    {
        *PickedContentFolder = SelectedPath;
    });

    TSharedRef<SWindow> PickerWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Bake & Save Animation To")))
        .ClientSize(FVector2D(450, 400))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    PickerWindow->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            ContentBrowser.CreatePathPicker(PathPickerConfig)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("OK")))
                    .OnClicked_Lambda([this, PickerWindow, PickedContentFolder, AnimName, SourceAnimPath]()
                    {
                        const FString DestinationFolder = PickedContentFolder.IsValid()
                            ? PickedContentFolder->Replace(TEXT("//"), TEXT("/"))
                            : FString();

                        PickerWindow->RequestDestroyWindow();

                        if (DestinationFolder.IsEmpty())
                            return FReply::Handled();

                        BakeSaveToFolder = DestinationFolder;
                        SaveSettings();

                        FEditingSessionSequencerHelper::BakeAndSaveAnimation(AnimName, SourceAnimPath, DestinationFolder);
                        return FReply::Handled();
                    })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Cancel")))
                    .OnClicked_Lambda([PickerWindow]()
                    {
                        PickerWindow->RequestDestroyWindow();
                        return FReply::Handled();
                    })
            ]
        ]
    );

    FSlateApplication::Get().AddWindow(PickerWindow);
    return FReply::Handled();
}

FReply SEditingSessionWindow::OnCheckpointCurrentAnimation()
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(FSeqQueue::Get().GetCurrentIndex()))
        return FReply::Handled();

    const FQueuedAnim CurrentAnim = All[FSeqQueue::Get().GetCurrentIndex()];
    const FString SourceAnimPath = CurrentAnim.Path.ToString();

    UObject* SourceAsset = UEditorAssetLibrary::LoadAsset(SourceAnimPath);
    if (!SourceAsset)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                FText::FromString(TEXT("Failed to load source animation asset:\n\n{0}")),
                FText::FromString(SourceAnimPath)
            )
        );
        return FReply::Handled();
    }

    const FString DefaultCheckpointFolder = TEXT("/Game/ToucanTemp/Checkpoints");
    if (!UEditorAssetLibrary::DoesDirectoryExist(DefaultCheckpointFolder))
    {
        UEditorAssetLibrary::MakeDirectory(DefaultCheckpointFolder);
    }

    TSharedPtr<FString> PickedContentFolder = MakeShared<FString>(DefaultCheckpointFolder);

    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

    FPathPickerConfig PathPickerConfig;
    PathPickerConfig.DefaultPath = DefaultCheckpointFolder;
    PathPickerConfig.bAllowContextMenu = true;
    PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([PickedContentFolder](const FString& SelectedPath)
    {
        *PickedContentFolder = SelectedPath;
    });

    TSharedRef<SWindow> PickerWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Save Checkpoint Sequence")))
        .ClientSize(FVector2D(450, 400))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    PickerWindow->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            ContentBrowser.CreatePathPicker(PathPickerConfig)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("OK")))
                    .OnClicked_Lambda([this, PickerWindow, PickedContentFolder, SourceAnimPath]()
                    {
                        const FString CheckpointPath = FEditingSessionSequencerHelper::SaveCheckpointForCurrentSequence(
                            SourceAnimPath,
                            *PickedContentFolder);

                        if (CheckpointPath.IsEmpty())
                        {
                            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to save checkpoint sequence.")));
                            return FReply::Handled();
                        }

                        UObject* Asset = UEditorAssetLibrary::LoadAsset(SourceAnimPath);
                        if (Asset)
                        {
                            UEditorAssetLibrary::SetMetadataTag(Asset, TEXT("Processed"), TEXT("False"));
                            UEditorAssetLibrary::SetMetadataTag(Asset, TEXT("Checkpointed"), TEXT("True"));
                            UEditorAssetLibrary::SetMetadataTag(Asset, TEXT("CheckpointPath"), CheckpointPath);
                            UEditorAssetLibrary::SaveLoadedAsset(Asset, false);
                        }

                        FSeqQueue::Get().SetProcessed(FSoftObjectPath(SourceAnimPath), false);
                        FSeqQueue::Get().SetCheckpoint(FSoftObjectPath(SourceAnimPath), CheckpointPath);

                        PickerWindow->RequestDestroyWindow();
                        RefreshQueue();
                        return FReply::Handled();
                    })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Cancel")))
                    .OnClicked_Lambda([PickerWindow]()
                    {
                        PickerWindow->RequestDestroyWindow();
                        return FReply::Handled();
                    })
            ]
        ]);

    FSlateApplication::Get().AddWindow(PickerWindow);
    return FReply::Handled();
}

FReply SEditingSessionWindow::OnLoadNextAnimation()
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (All.Num() == 0)
        return FReply::Handled();

    // Determine next index
    const int32 CurrentIndex = FSeqQueue::Get().GetCurrentIndex();
    const int32 StartIndex = CurrentIndex == INDEX_NONE ? -1 : CurrentIndex;

    int32 NextUnprocessedIndex = INDEX_NONE;

    for (int32 Offset = 1; Offset <= All.Num(); ++Offset)
    {
        const int32 CandidateIndex = (StartIndex + Offset + All.Num()) % All.Num();

        if (!IsQueuedAnimProcessed(All[CandidateIndex]))
        {
            NextUnprocessedIndex = CandidateIndex;
            break;
        }
    }

    if (NextUnprocessedIndex == INDEX_NONE)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(TEXT("No unprocessed animations found."))
        );
        return FReply::Handled();
    }

    FSeqQueue::Get().SetCurrentIndex(NextUnprocessedIndex);

    bool bCheckpointPromptAlreadyShown = false;
    FString CheckpointPath;
    if (TryGetCheckpointPath(All[FSeqQueue::Get().GetCurrentIndex()], CheckpointPath))
    {
        bCheckpointPromptAlreadyShown = true;
        const EAppReturnType::Type Response = FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::Format(
                FText::FromString(TEXT("This animation has a checkpoint:\n\n{0}\n\nUse the checkpoint?\n\nYes: continue from checkpoint\nNo: start over from the source animation")),
                FText::FromString(CheckpointPath)
            ),
            FText::FromString(TEXT("Checkpoint Found")));

        if (Response == EAppReturnType::Yes)
        {
            ContinueFromCheckpointAtIndex(FSeqQueue::Get().GetCurrentIndex());
            return FReply::Handled();
        }
    }

    // Load animation asset
    UObject* AnimObject = All[FSeqQueue::Get().GetCurrentIndex()].Path.TryLoad();
    
    // Notify if failed to load
    if (!AnimObject)
    {
        const FString AnimPath = All[FSeqQueue::Get().GetCurrentIndex()].Path.ToString();

        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                FText::FromString(TEXT("Failed to load animation asset:\n\n{0}\n\nThe queue item may point to a missing or invalid asset.")),
                FText::FromString(AnimPath)
            )
        );

        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to load animation asset: %s"), *AnimPath);
        return FReply::Handled();
    }

    SyncQueueStatusFromLoadedAsset(All[FSeqQueue::Get().GetCurrentIndex()], AnimObject);
    if (!bCheckpointPromptAlreadyShown &&
        TryGetCheckpointPath(FSeqQueue::Get().GetAll()[FSeqQueue::Get().GetCurrentIndex()], CheckpointPath))
    {
        const EAppReturnType::Type Response = FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::Format(
                FText::FromString(TEXT("This animation has a checkpoint:\n\n{0}\n\nUse the checkpoint?\n\nYes: continue from checkpoint\nNo: start over from the source animation")),
                FText::FromString(CheckpointPath)
            ),
            FText::FromString(TEXT("Checkpoint Found")));

        if (Response == EAppReturnType::Yes)
        {
            ContinueFromCheckpointAtIndex(FSeqQueue::Get().GetCurrentIndex());
            return FReply::Handled();
        }
    }

    if (FSeqQueue::Get().GetAll()[FSeqQueue::Get().GetCurrentIndex()].bProcessed)
    {
        const FString AnimName = AnimObject->GetName();
        const FString AnimPath = FSeqQueue::Get().GetAll()[FSeqQueue::Get().GetCurrentIndex()].Path.ToString();

        const EAppReturnType::Type Response = FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::Format(
                FText::FromString(TEXT("Animation:\t'{0}'\nPath:\t'{1}'\nNote:\tis marked as 'Processed'.\nAction:\tdo you want to load it anyway?")),
                FText::FromString(AnimName),
                FText::FromString(AnimPath)
            ),
            FText::FromString(TEXT("Processed Animation Detected")));

        if (Response == EAppReturnType::No)
        {
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
    LoadBestMatchedVideoForCurrent();

    return FReply::Handled();
}

FReply SEditingSessionWindow::OnLoadVideoForCurrent()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] DesktopPlatform unavailable; cannot open video picker."));
        return FReply::Handled();
    }

    TArray<FString> SelectedFiles;
    const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    const FString FileTypes = TEXT("Video Files (*.mov;*.mp4;*.mxf;*.avi)|*.mov;*.mp4;*.mxf;*.avi|All Files (*.*)|*.*");

    const bool bSelected = DesktopPlatform->OpenFileDialog(
        ParentWindowHandle,
        TEXT("Select video for current animation"),
        FPaths::ProjectDir(),
        TEXT(""),
        FileTypes,
        0,
        SelectedFiles
    );

    if (bSelected && SelectedFiles.Num() > 0)
    {
        FEditingSessionSequencerHelper::LoadVideoForCurrentSequence(SelectedFiles[0]);
    }

    return FReply::Handled();
}

FString SEditingSessionWindow::FindBestMatchedVideoForCurrent() const
{
    if (SelectedVideoFolder.IsEmpty() || !FPaths::DirectoryExists(SelectedVideoFolder))
    {
        return FString();
    }

    const auto& All = FSeqQueue::Get().GetAll();
    const int32 CurrentIndex = FSeqQueue::Get().GetCurrentIndex();
    if (!All.IsValidIndex(CurrentIndex))
    {
        return FString();
    }

    const FQueuedAnim& CurrentAnim = All[CurrentIndex];
    const FString QueueName = CurrentAnim.DisplayName.IsEmpty()
        ? CurrentAnim.Path.GetAssetName()
        : CurrentAnim.DisplayName.ToString();

    TArray<FString> CandidateFiles;
    IFileManager::Get().IterateDirectoryRecursively(*SelectedVideoFolder, [&CandidateFiles](const TCHAR* FilenameOrDirectory, const bool bIsDirectory)
    {
        if (bIsDirectory)
        {
            return true;
        }

        const FString CandidateFile(FilenameOrDirectory);
        const FString Extension = FPaths::GetExtension(CandidateFile).ToLower();
        if (Extension == TEXT("mp4") || Extension == TEXT("mov") || Extension == TEXT("mxf") || Extension == TEXT("avi"))
        {
            CandidateFiles.Add(CandidateFile);
        }

        return true;
    });

    if (CandidateFiles.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No video files found in %s."), *SelectedVideoFolder);
        return FString();
    }

    FString BestFile;
    int32 BestScore = 0;
    for (const FString& CandidateFile : CandidateFiles)
    {
        const int32 Score = ScoreVideoNameMatch(QueueName, FPaths::GetBaseFilename(CandidateFile));
        if (Score > BestScore)
        {
            BestScore = Score;
            BestFile = CandidateFile;
        }
    }

    constexpr int32 MinimumMatchScore = 55;
    if (BestScore < MinimumMatchScore)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No matching video found for '%s' in %s. Best score was %d."),
            *QueueName,
            *SelectedVideoFolder,
            BestScore);
        return FString();
    }

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Matched video for '%s': %s (score %d)"),
        *QueueName,
        *BestFile,
        BestScore);
    return BestFile;
}

bool SEditingSessionWindow::LoadBestMatchedVideoForCurrent()
{
    const FString MatchedVideo = FindBestMatchedVideoForCurrent();
    if (MatchedVideo.IsEmpty())
    {
        return false;
    }

    FEditingSessionSequencerHelper::LoadVideoForCurrentSequence(MatchedVideo);
    return true;
}

void SEditingSessionWindow::ContinueFromCheckpointAtIndex(int32 TargetIndex)
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(TargetIndex))
        return;

    FString CheckpointPath;
    if (!TryGetCheckpointPath(All[TargetIndex], CheckpointPath))
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(TEXT("This queue item does not have a valid checkpoint sequence."))
        );
        return;
    }

    FSeqQueue::Get().SetCurrentIndex(TargetIndex);

    if (ListView.IsValid())
        ListView->RebuildList();

    if (!FEditingSessionSequencerHelper::OpenCheckpointSequence(CheckpointPath))
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                FText::FromString(TEXT("Failed to open checkpoint sequence:\n\n{0}")),
                FText::FromString(CheckpointPath)
            )
        );
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Continued queue item %d from checkpoint: %s"), TargetIndex, *CheckpointPath);
}

void SEditingSessionWindow::LoadAnimationAtIndex(int32 TargetIndex)
{
    const auto& All = FSeqQueue::Get().GetAll();
    if (!All.IsValidIndex(TargetIndex))
        return;

    FSeqQueue::Get().SetCurrentIndex(TargetIndex);

    if (ListView.IsValid())
        ListView->RebuildList();

    UObject* AnimObject = All[FSeqQueue::Get().GetCurrentIndex()].Path.TryLoad();

    if (!AnimObject)
    {
        const FString AnimPath = All[FSeqQueue::Get().GetCurrentIndex()].Path.ToString();

        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                FText::FromString(TEXT("Failed to load animation asset:\n\n{0}\n\nThe queue item may point to a missing or invalid asset.")),
                FText::FromString(AnimPath)
            )
        );

        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to load animation asset: %s"), *AnimPath);
        return;
    }

    SyncQueueStatusFromLoadedAsset(All[FSeqQueue::Get().GetCurrentIndex()], AnimObject);

    FString CheckpointPath;
    if (TryGetCheckpointPath(FSeqQueue::Get().GetAll()[FSeqQueue::Get().GetCurrentIndex()], CheckpointPath))
    {
        const EAppReturnType::Type Response = FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::Format(
                FText::FromString(TEXT("This animation has a checkpoint:\n\n{0}\n\nUse the checkpoint?\n\nYes: continue from checkpoint\nNo: start over from the source animation")),
                FText::FromString(CheckpointPath)
            ),
            FText::FromString(TEXT("Checkpoint Found")));

        if (Response == EAppReturnType::Yes)
        {
            ContinueFromCheckpointAtIndex(FSeqQueue::Get().GetCurrentIndex());
            return;
        }
    }

    // Check for "Processed" metadata
    if (FSeqQueue::Get().GetAll()[FSeqQueue::Get().GetCurrentIndex()].bProcessed)
    {
        const FString AnimName = AnimObject->GetName();
        const FString AnimPath = All[FSeqQueue::Get().GetCurrentIndex()].Path.ToString();

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
    LoadBestMatchedVideoForCurrent();

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Loaded animation at index %d: %s"), FSeqQueue::Get().GetCurrentIndex(), *Anim->GetName());
}

FString SEditingSessionWindow::GetCurrentConfiguredOutputFolder() const
{
    return FOutputHelper::Get();
}

void SEditingSessionWindow::ExportAnimSequencesToFolder(const FString& sourceContentFolder, const FString& outputDiskFolder) const
{
    FARFilter filter;
    filter.PackagePaths.Add(*sourceContentFolder);
    filter.bRecursivePaths = true;
    filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());

    FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FAssetData> assetDataList;
    assetRegistryModule.Get().GetAssets(filter, assetDataList);

    if (assetDataList.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] No AnimSequence assets found under %s"), *sourceContentFolder);
        return;
    }

    IFileManager::Get().MakeDirectory(*outputDiskFolder, true);

    int32 exportedCount = 0;

    for (const FAssetData& assetData : assetDataList)
    {
        UAnimSequence* animSequence = Cast<UAnimSequence>(assetData.GetAsset());
        if (!animSequence)
            continue;

        const FString exportFilename = FPaths::Combine(outputDiskFolder, animSequence->GetName() + TEXT(".fbx"));

        UAssetExportTask* exportTask = NewObject<UAssetExportTask>();
        exportTask->Object = animSequence;
        exportTask->Filename = exportFilename;
        exportTask->bSelected = false;
        exportTask->bReplaceIdentical = true;
        exportTask->bPrompt = false;
        exportTask->bAutomated = true;
        exportTask->bUseFileArchive = false;
        exportTask->Exporter = NewObject<UAnimSequenceExporterFBX>();

        UFbxExportOption* exportOptions = NewObject<UFbxExportOption>();
        exportOptions->FbxExportCompatibility = EFbxExportCompatibility::FBX_2020;
        exportOptions->bASCII = false;
        exportOptions->bForceFrontXAxis = false;
        exportOptions->VertexColor = false;
        exportOptions->LevelOfDetail = false;
        exportOptions->Collision = false;
        exportOptions->bExportSourceMesh = false;
        exportOptions->bExportMorphTargets = true;
        exportOptions->bExportPreviewMesh = false;
        exportOptions->MapSkeletalMotionToRoot = false;
        exportOptions->bExportLocalTime = true;
        exportTask->Options = exportOptions;

        if (UExporter::RunAssetExportTask(exportTask))
        {
            ++exportedCount;
            UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Exported %s -> %s"), *animSequence->GetPathName(), *exportFilename);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[ToucanSequencer] Failed to export %s"), *animSequence->GetPathName());
        }
    }

    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Exported %d animation(s) from %s to %s"), exportedCount, *sourceContentFolder, *outputDiskFolder);
}

FReply SEditingSessionWindow::OnExportFolder()
{
    FContentBrowserModule& contentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    IContentBrowserSingleton& contentBrowser = contentBrowserModule.Get();

    TSharedPtr<FString> pickedContentFolder = MakeShared<FString>(GetCurrentConfiguredOutputFolder());

    FPathPickerConfig pathPickerConfig;
    pathPickerConfig.DefaultPath = GetCurrentConfiguredOutputFolder();
    pathPickerConfig.bAllowContextMenu = true;
    pathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([pickedContentFolder](const FString& selectedPath)
    {
        *pickedContentFolder = selectedPath;
    });

    TSharedRef<SWindow> pickerWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Select Source Animation Folder")))
        .ClientSize(FVector2D(450, 400))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    pickerWindow->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            contentBrowser.CreatePathPicker(pathPickerConfig)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("OK")))
                .OnClicked_Lambda([this, pickerWindow, pickedContentFolder]()
                {
                    const FString sourceContentFolder = *pickedContentFolder;
                    pickerWindow->RequestDestroyWindow();

                    if (sourceContentFolder.IsEmpty())
                        return FReply::Handled();

                    IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
                    if (!desktopPlatform)
                        return FReply::Handled();

                    const void* parentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

                    FString outputDiskFolder;
                    const bool didChooseDiskFolder = desktopPlatform->OpenDirectoryDialog(
                        parentWindowHandle,
                        TEXT("Select FBX Output Folder"),
                        FPaths::ProjectDir(),
                        outputDiskFolder
                    );

                    if (didChooseDiskFolder && !outputDiskFolder.IsEmpty())
                    {
                        ExportAnimSequencesToFolder(sourceContentFolder, outputDiskFolder);
                    }

                    return FReply::Handled();
                })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Cancel")))
                .OnClicked_Lambda([pickerWindow]()
                {
                    pickerWindow->RequestDestroyWindow();
                    return FReply::Handled();
                })
            ]
        ]
    );

    FSlateApplication::Get().AddWindow(pickerWindow);
    return FReply::Handled();
}
