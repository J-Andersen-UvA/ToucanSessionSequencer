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
    // --- internal helpers ---
    void RefreshQueue();
    TSharedRef<ITableRow> OnMakeRow(TSharedPtr<FQueuedAnim> Item, const TSharedRef<STableViewBase>& Owner);

    FReply OnSelectSkeletalMesh();
    FReply OnSelectRig();
    FReply OnLoadNextAnimation();

private:
    // --- data ---
    TArray<TSharedPtr<FQueuedAnim>> Rows;
    TSharedPtr<SListView<TSharedPtr<FQueuedAnim>>> ListView;

    int32 CurrentIndex = 0; // highlighted animation index

    TSoftObjectPtr<USkeletalMesh> SelectedMesh;
    TSoftObjectPtr<UObject> SelectedRig; // generic placeholder (ControlRig or BP)
};
