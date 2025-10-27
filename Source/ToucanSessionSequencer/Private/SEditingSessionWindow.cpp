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

void SEditingSessionWindow::Construct(const FArguments&)
{
    RefreshQueue();

    ChildSlot
        [
            SNew(SBorder).Padding(8)
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildSelectionRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildStatusRow()]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator).Thickness(3.0f)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[BuildSessionControlsRow()]
                        + SVerticalBox::Slot().FillHeight(1.f)[BuildQueueList()]
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
        SNew(STextBlock)
            .Text_Lambda([this, Item]() {
            int32 RowIndex = Rows.IndexOfByKey(Item);
            FString Label = Item->DisplayName.ToString();
            if (RowIndex == CurrentIndex)
            {
                Label += TEXT("  <-- editing");
            }
            return FText::FromString(Label);
                })
            .ColorAndOpacity_Lambda([this, Item]() {
            int32 RowIndex = Rows.IndexOfByKey(Item);
            return (RowIndex == CurrentIndex)
                ? FLinearColor(0.2f, 0.4f, 1.0f)
                : FLinearColor::White;
                })
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
    const FQueuedAnim& SelectedAnim = All[CurrentIndex];
    UAnimSequence* Anim = Cast<UAnimSequence>(SelectedAnim.Path.TryLoad());
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
