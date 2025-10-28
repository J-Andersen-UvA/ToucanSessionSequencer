#pragma once
#include "Widgets/SCompoundWidget.h"

class SMidiMappingWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMidiMappingWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedPtr<SListView<TSharedPtr<FString>>> MappingList;

    FReply OnLearnClicked(FName ActionName);
    FReply OnUnbindClicked(FName ActionName);
    FReply OnSaveClicked();

    void RefreshMappings();

    // Simple model struct
    struct FControlRow
    {
        FString ActionName;
        FString TargetControl;
        FString MidiId;
        FString Modus;
    };
    TArray<TSharedPtr<FControlRow>> Rows;
};
