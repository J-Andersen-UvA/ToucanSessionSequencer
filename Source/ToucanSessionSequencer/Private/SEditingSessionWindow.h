#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "SeqQueue.h"

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
    TSharedRef<SWidget> BuildLoadButton();
    TSharedRef<SWidget> BuildQueueList();

private:
    // --- internal helpers ---
    void RefreshQueue();
    TSharedRef<ITableRow> OnMakeRow(TSharedPtr<FQueuedAnim> Item, const TSharedRef<STableViewBase>& Owner);

    FReply OnSelectSkeletalMesh();
    FReply OnSelectRig();
    FReply OnSelectOutputFolder();
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
    static constexpr const TCHAR* OutputFolderKey = TEXT("LastSelectedOutputFolder");
    FString OutputFolder = TEXT("/Game/ToucanTemp/Output");

    void LoadSettings();
    void SaveSettings() const;
};
