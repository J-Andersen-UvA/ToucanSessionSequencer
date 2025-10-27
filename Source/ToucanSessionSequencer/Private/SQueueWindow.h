#pragma once
#include "CoreMinimal.h"
#include "SeqQueue.h"
#include "Widgets/SCompoundWidget.h"

class SQueueWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SQueueWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&);

private:
    TArray<TSharedPtr<FQueuedAnim>> Rows;
    TSharedRef<ITableRow> OnMakeRow(TSharedPtr<FQueuedAnim> Item, const TSharedRef<STableViewBase>& Owner);
    void Refresh();

private:
    TSharedPtr<SListView<TSharedPtr<FQueuedAnim>>> List;
};
