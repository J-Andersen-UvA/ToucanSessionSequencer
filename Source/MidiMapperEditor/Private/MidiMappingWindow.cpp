#include "MidiMappingWindow.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMidiMappingWindow::Construct(const FArguments& InArgs)
{
    RefreshMappings();

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(5)
        [
            SNew(SButton)
            .Text(FText::FromString("Save Mappings"))
            .OnClicked(this, &SMidiMappingWindow::OnSaveClicked)
        ]

        + SVerticalBox::Slot().FillHeight(1.0f).Padding(5)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    // Later: dynamic list
                    SNew(STextBlock).Text(FText::FromString("Mappings go here"))
                ]
            ]
        ]
    ];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMidiMappingWindow::RefreshMappings()
{
    Rows.Empty();
    // TODO: read from UMidiMappingManager
}

FReply SMidiMappingWindow::OnLearnClicked(FName ActionName)
{
    // TODO: connect temporary delegate to UMidiEventRouter for learn mode
    return FReply::Handled();
}

FReply SMidiMappingWindow::OnUnbindClicked(FName ActionName)
{
    // TODO: call UMidiMappingManager->UnregisterMapping(...)
    return FReply::Handled();
}

FReply SMidiMappingWindow::OnSaveClicked()
{
    if (UMidiMappingManager* Manager = UMidiMappingManager::Get())
        Manager->SaveMappings();
    return FReply::Handled();
}
