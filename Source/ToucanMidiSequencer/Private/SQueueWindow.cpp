#include "SQueueWindow.h"
#include "SeqQueue.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

void SQueueWindow::Construct(const FArguments&)
{
    Refresh();

    ChildSlot
    [
        SNew(SBorder).Padding(8)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SAssignNew(List, SListView<TSharedPtr<FQueuedAnim>>)
                .ListItemsSource(&Rows)
                .OnGenerateRow(this, &SQueueWindow::OnMakeRow)
                .SelectionMode(ESelectionMode::None)
            ]
        ]
    ];
}

void SQueueWindow::Refresh()
{
    Rows.Reset();
    for (const auto& Q : FSeqQueue::Get().GetAll())
    {
        Rows.Add(MakeShared<FQueuedAnim>(Q));
    }
    if (List.IsValid()) List->RequestListRefresh();
}

TSharedRef<ITableRow> SQueueWindow::OnMakeRow(
    TSharedPtr<FQueuedAnim> Item, const TSharedRef<STableViewBase>& Owner)
{
    return SNew(STableRow<TSharedPtr<FQueuedAnim>>, Owner)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(Item.IsValid() ? Item->DisplayName : FText::GetEmpty())
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Remove")))
            .OnClicked_Lambda([this, Item]()
            {
                int32 Index = INDEX_NONE;
                const auto& All = FSeqQueue::Get().GetAll();
                if (Item.IsValid())
                {
                    // find by path
                    for (int32 i=0;i<All.Num();++i)
                    {
                        if (All[i].Path == Item->Path) { Index = i; break; }
                    }
                }
                if (Index != INDEX_NONE)
                {
                    FSeqQueue::Get().RemoveAt(Index);
                    Refresh();
                }
                return FReply::Handled();
            })
        ]
    ];
}
