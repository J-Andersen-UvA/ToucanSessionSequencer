#include "SQueueWindow.h"
#include "SeqQueue.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "EditorStyleSet.h"

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
    FSlateColor TextColor = FSlateColor::UseForeground();
    FText ExtraText = FText::GetEmpty();

    if (Item.IsValid())
    {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(Item->Path.ToString());
        if (Asset)
        {
            // Read the "Processed" metadata tag
            const FString TagValue = UEditorAssetLibrary::GetMetadataTag(Asset, TEXT("Processed"));

            if (TagValue.Equals(TEXT("True"), ESearchCase::IgnoreCase))
            {
                TextColor = FSlateColor(FLinearColor::Red);
                ExtraText = FText::FromString(TEXT(" (Already processed?)"));
            }
        }
    }


    return SNew(STableRow<TSharedPtr<FQueuedAnim>>, Owner)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(Item.IsValid()
                            ? FText::Format(FText::FromString("{0}{1}"), Item->DisplayName, ExtraText)
                            : FText::GetEmpty())
                        .ColorAndOpacity(TextColor)
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Remove")))
                        .OnClicked_Lambda([this, Item]()
                            {
                                int32 Index = INDEX_NONE;
                                const auto& All = FSeqQueue::Get().GetAll();
                                if (Item.IsValid())
                                {
                                    for (int32 i = 0; i < All.Num(); ++i)
                                    {
                                        if (All[i].Path == Item->Path)
                                        {
                                            Index = i;
                                            break;
                                        }
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
