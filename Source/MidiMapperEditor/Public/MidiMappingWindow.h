#pragma once
#include "Widgets/SCompoundWidget.h"
#include "ControlRig.h"

class SMidiMappingWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMidiMappingWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // header state
    FString ActiveRigName;
    FString ActiveDeviceName;

    // Device list
    TArray<TSharedPtr<FString>> AvailableDevices;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> DeviceCombo;

    // table model
    struct FControlRow
    {
        FString ActionName;
        FString TargetControl;
        FString Modus;
        int32 BoundControlId;
    };
    TArray<TSharedPtr<FControlRow>> Rows;
    TSharedPtr<SListView<TSharedPtr<FControlRow>>> MappingListView;

    // helpers
    void RefreshRigAndRows();    // fetch rig + static actions
    void RefreshBindings();      // read manager.GetAll() into Rows
    void SetActiveDevice(const FString& Device);
    void PopulateFromRig(UControlRig* ControlRig);

    // UI callbacks
    TSharedRef<ITableRow> GenerateMappingRow(
        TSharedPtr<FControlRow> InItem,
        const TSharedRef<STableViewBase>& OwnerTable);
    FReply OnLearnClicked(TSharedPtr<FControlRow> Row);
    FReply OnUnbindClicked(TSharedPtr<FControlRow> Row);
    void OnLearnedControl(int32 ControlId, TSharedPtr<FControlRow> Row);
};
