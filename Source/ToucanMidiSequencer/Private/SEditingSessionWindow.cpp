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

            // --- Skeletal mesh & rig selection ---
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Select Skeletal Mesh")))
                    .OnClicked(this, &SEditingSessionWindow::OnSelectSkeletalMesh)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Select Rig")))
                    .OnClicked(this, &SEditingSessionWindow::OnSelectRig)
                ]
            ]

            // --- current selections display ---
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() {
                    FString MeshName = SelectedMesh.IsValid() ? SelectedMesh->GetName() : TEXT("None");
                    FString RigName  = SelectedRig.IsValid()  ? SelectedRig->GetName()  : TEXT("None");
                    return FText::FromString(FString::Printf(TEXT("Mesh: %s   |   Rig: %s"), *MeshName, *RigName));
                })
            ]

            // --- Load next animation ---
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Load Next Animation")))
                .OnClicked(this, &SEditingSessionWindow::OnLoadNextAnimation)
            ]

            // --- Queue list ---
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SAssignNew(ListView, SListView<TSharedPtr<FQueuedAnim>>)
                .ListItemsSource(&Rows)
                .OnGenerateRow(this, &SEditingSessionWindow::OnMakeRow)
                .SelectionMode(ESelectionMode::None)
            ]
        ]
    ];

    LoadSettings();
}

void SEditingSessionWindow::LoadSettings()
{
#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif

    FString MeshPath, RigPath;
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

    if (SelectedRig.IsValid())
        GConfig->SetString(CfgSection, RigKey, *SelectedRig.ToSoftObjectPath().ToString(), Ini);

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

    // Delegate to helper
    FEditingSessionSequencerHelper::LoadNextAnimation(SelectedMesh, SelectedRig, Anim);

    return FReply::Handled();
}
