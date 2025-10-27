#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "SeqQueue.h"
#include "OutputHelper.h"

/**
 * Editing Session main window.
 * Lets you pick SkeletalMesh & Rig and step through queued animations.
 */
class SEditingSessionWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEditingSessionWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> BuildSelectionRow();
    TSharedRef<SWidget> BuildStatusRow();
    TSharedRef<SWidget> BuildQueueAdditionControlsRow();
    TSharedRef<SWidget> BuildQueueRemovalControlsRow();
    TSharedRef<SWidget> BuildSessionControlsRow();
    TSharedRef<SWidget> BuildQueueList();

private:
    // --- internal helpers ---
    void RefreshQueue();
    TSharedRef<ITableRow> OnMakeRow(TSharedPtr<FQueuedAnim> Item, const TSharedRef<STableViewBase>& Owner);

    FReply OnSelectSkeletalMesh();
    FReply OnSelectRig();
    FReply OnSelectOutputFolder();
    FReply OnBakeSaveAnimation();
    FReply OnLoadNextAnimation();

private:
    TArray<TSharedPtr<FQueuedAnim>> Rows;
    TSharedPtr<SListView<TSharedPtr<FQueuedAnim>>> ListView;

    int32 CurrentIndex = INDEX_NONE;

    TSoftObjectPtr<USkeletalMesh> SelectedMesh;
    TSoftObjectPtr<UObject> SelectedRig;

    // Config keys
    static constexpr const TCHAR* CfgSection = TEXT("ToucanEditingSession");
    static constexpr const TCHAR* MeshKey = TEXT("LastSelectedMesh");
    static constexpr const TCHAR* RigKey = TEXT("LastSelectedRig");

    void LoadSettings();
    void SaveSettings() const;
};
