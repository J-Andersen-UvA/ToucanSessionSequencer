#include "SEditingSessionWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"

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
    int32 RowIndex = Rows.IndexOfByKey(Item);
    FSlateColor RowColor = (RowIndex == CurrentIndex)
        ? FSlateColor(FLinearColor(0.2f, 0.4f, 1.0f)) // blue highlight
        : FSlateColor::UseForeground();

    return SNew(STableRow<TSharedPtr<FQueuedAnim>>, Owner)
    [
        SNew(STextBlock)
        .Text(Item->DisplayName)
        .ColorAndOpacity(RowColor)
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
                SelectedMesh = Cast<USkeletalMesh>(Selected[0].GetAsset());
        }),
        FOnAssetDialogCancelled::CreateLambda([] {})
    );

    return FReply::Handled();
}

FReply SEditingSessionWindow::OnSelectRig()
{
    FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

    FOpenAssetDialogConfig Config;
    Config.DialogTitleOverride = FText::FromString(TEXT("Select Rig Blueprint"));
    Config.AssetClassNames.Add(UBlueprint::StaticClass()->GetClassPathName());
    Config.bAllowMultipleSelection = false;

    CB.Get().CreateOpenAssetDialog(
        Config,
        FOnAssetsChosenForOpen::CreateLambda([this](const TArray<FAssetData>& Selected)
        {
            if (Selected.Num() > 0)
                SelectedRig = Selected[0].GetAsset();
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

    CurrentIndex = (CurrentIndex + 1) % All.Num();
    if (ListView.IsValid())
        ListView->RequestListRefresh();

    // Later we’ll trigger event/midi binding here
    UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] LoadNextAnimation called, new index: %d"), CurrentIndex);
    return FReply::Handled();
}
