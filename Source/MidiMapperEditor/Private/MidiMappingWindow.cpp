#include "MidiMappingWindow.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "UnrealMidiSubsystem.h"
#include "MidiTypes.h"
#include "MidiMapperModule.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/SoftObjectPath.h"
#include "ControlRig.h"
#define GRouter FMidiMapperModule::GetRouter()

void SMidiMappingWindow::Construct(const FArguments& InArgs)
{
    RefreshRigAndRows();

    if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
    {
        TArray<FUnrealMidiDeviceInfo> Devices;
        Midi->EnumerateDevices(Devices);
        for (const FUnrealMidiDeviceInfo& D : Devices)
        {
            if (D.bIsInput) // only show inputs
            {
                AvailableDevices.Add(MakeShared<FString>(D.Name));
            }
        }
        if (AvailableDevices.Num() > 0)
            ActiveDeviceName = *AvailableDevices[0];
    }

    // Fill static control rows
    Rows = {
        MakeShared<FControlRow>(FControlRow{TEXT("Queue Load Next"), TEXT("Queue.LoadNext"), TEXT("Trigger"), -1}),
        MakeShared<FControlRow>(FControlRow{TEXT("Queue Bake & Save"), TEXT("Queue.BakeSave"), TEXT("Trigger"), -1}),
        MakeShared<FControlRow>(FControlRow{TEXT("Sequencer Next Frame"), TEXT("Sequencer.NextFrame"), TEXT("Trigger"), -1}),
        MakeShared<FControlRow>(FControlRow{TEXT("Rig Ctrl Index 1"), TEXT("Rig.Index1"), TEXT("Analog"), -1})
    };

    // Layout
    ChildSlot
    [
        SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(5)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("Current rig: %s"), *ActiveRigName)))
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(5)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromString("Device:"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
                    [
                        SAssignNew(DeviceCombo, SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&AvailableDevices)
                            .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
                                {
                                    if (NewItem.IsValid())
                                        ActiveDeviceName = *NewItem;
                                })
                            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                {
                                    return SNew(STextBlock).Text(FText::FromString(*Item));
                                })
                            [
                                SNew(STextBlock).Text_Lambda([this] { return FText::FromString(ActiveDeviceName); })
                            ]
                    ]
            ]

        + SVerticalBox::Slot().FillHeight(1.0f).Padding(5)
            [
                SAssignNew(MappingListView, SListView<TSharedPtr<FControlRow>>)
                    .ListItemsSource(&Rows)
                    .SelectionMode(ESelectionMode::None)
                    .OnGenerateRow(this, &SMidiMappingWindow::GenerateMappingRow)
                    .HeaderRow
                    (
                        SNew(SHeaderRow)
                        + SHeaderRow::Column("Control").DefaultLabel(FText::FromString("Control")).FillWidth(0.4f)
                        + SHeaderRow::Column("Learn").DefaultLabel(FText::FromString("Learn")).FillWidth(0.2f)
                        + SHeaderRow::Column("Mapping").DefaultLabel(FText::FromString("Current Mapping")).FillWidth(0.4f)
                    )
            ]
    ];

    // simple manual widget creation: replace with SListView later
    // Render rows now:
    RefreshBindings();
}

void SMidiMappingWindow::RefreshRigAndRows()
{
    // TODO: query ToucanSessionSequencer for active rig
    FString RigPath;
    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), RigPath, GEditorPerProjectIni);

    if (RigPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MidiMapping] No rig path found in config"));
        ActiveRigName = TEXT("None");
        return;
    }

    TSoftObjectPtr<UObject> RigAsset = TSoftObjectPtr<UObject>(FSoftObjectPath(RigPath));
    UObject* Loaded = RigAsset.IsValid() ? RigAsset.Get() : RigAsset.LoadSynchronous();
    if (Loaded)
    {
        ActiveRigName = Loaded->GetName();
        UE_LOG(LogTemp, Display, TEXT("[MidiMapping] Loaded rig from config: %s"), *RigPath);
    }
    else
    {
        ActiveRigName = TEXT("Invalid");
        UE_LOG(LogTemp, Warning, TEXT("[MidiMapping] Failed to load rig from config: %s"), *RigPath);
    }

    Rows.Empty();
    Rows = {
    MakeShared<FControlRow>(FControlRow{TEXT("Queue Load Next"), TEXT("")}),
    MakeShared<FControlRow>(FControlRow{TEXT("Queue Bake & Save"), TEXT("")}),
    MakeShared<FControlRow>(FControlRow{TEXT("Sequencer Next Frame"), TEXT("")}),
    MakeShared<FControlRow>(FControlRow{TEXT("Rig Ctrl Index 1"), TEXT("")})
    };

    // Static examples; replace with real control list
    auto Add = [this](const TCHAR* Name, const TCHAR* Target, const TCHAR* Modus)
        {
            auto R = MakeShared<FControlRow>();
            R->ActionName = Name; R->TargetControl = Target; R->Modus = Modus; R->BoundControlId = -1;
            Rows.Add(R);
        };
    Add(TEXT("Queue_LoadNext"), TEXT("Queue.LoadNext"), TEXT("Trigger"));
    Add(TEXT("Queue_BakeSave"), TEXT("Queue.BakeSave"), TEXT("Trigger"));
    Add(TEXT("Sequencer_NextFrame"), TEXT("Sequencer.NextFrame"), TEXT("Trigger"));
    Add(TEXT("Rig_Ctrl_Index1"), TEXT("Rig.Index1"), TEXT("Float"));
}

void SMidiMappingWindow::SetActiveDevice(const FString& Device)
{
    ActiveDeviceName = Device;
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        M->Initialize(ActiveDeviceName, ActiveRigName);
    }
    RefreshBindings();
}

void SMidiMappingWindow::RefreshBindings()
{
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        // naive reverse lookup by ActionName+Target
        const auto& Map = M->GetAll();
        for (auto& Row : Rows)
        {
            Row->BoundControlId = -1;
            for (const auto& KVP : Map)
            {
                if (KVP.Value.ActionName.ToString() == Row->ActionName &&
                    KVP.Value.TargetControl.ToString() == Row->TargetControl)
                {
                    Row->BoundControlId = KVP.Key;
                    break;
                }
            }
        }
    }
}

FReply SMidiMappingWindow::OnLearnClicked(TSharedPtr<FControlRow> Row)
{
    UE_LOG(LogTemp, Log, TEXT("Learn clicked for: %s"), *Row->ActionName);
    return FReply::Handled();
}

void SMidiMappingWindow::OnLearnedControl(int32 ControlId, TSharedPtr<FControlRow> Row)
{
    if (!Row.IsValid()) return;
    if (UMidiMappingManager* M = UMidiMappingManager::Get())
    {
        FMidiMappedAction A;
        A.ActionName = FName(*Row->ActionName);
        A.TargetControl = FName(*Row->TargetControl);
        A.Modus = FName(*Row->Modus);
        M->RegisterOrUpdate(ControlId, A); // autosave
    }
    RefreshBindings();
    if (GRouter) GRouter->OnMidiLearn().RemoveAll(this);
}

FReply SMidiMappingWindow::OnUnbindClicked(TSharedPtr<FControlRow> Row)
{
    if (!Row.IsValid()) return FReply::Handled();
    if (Row->BoundControlId >= 0)
    {
        if (UMidiMappingManager* M = UMidiMappingManager::Get())
            M->RemoveMapping(Row->BoundControlId); // autosave
        Row->BoundControlId = -1;
        RefreshBindings();
    }
    return FReply::Handled();
}

TSharedRef<ITableRow> SMidiMappingWindow::GenerateMappingRow(
        TSharedPtr<FControlRow> InItem,
        const TSharedRef<STableViewBase>& OwnerTable)
{
    class SMidiMappingRow : public SMultiColumnTableRow<TSharedPtr<FControlRow>>
    {
    public:
        SLATE_BEGIN_ARGS(SMidiMappingRow) {}
            SLATE_ARGUMENT(TSharedPtr<FControlRow>, RowItem)
            SLATE_ARGUMENT(TWeakPtr<SMidiMappingWindow>, OwnerWindow)
        SLATE_END_ARGS()

        TSharedPtr<FControlRow> RowItem;
        TWeakPtr<SMidiMappingWindow> OwnerWindow;

        void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& TableView)
        {
            RowItem = InArgs._RowItem;
            OwnerWindow = InArgs._OwnerWindow;
            SMultiColumnTableRow<TSharedPtr<FControlRow>>::Construct(
                FSuperRowType::FArguments(), TableView);
        }

        virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
        {
            const FMargin CellPadding(5.f, 2.f);

            if (ColumnName == "Control")
            {
                return SNew(SBox).Padding(CellPadding)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(RowItem->ActionName))
                    ];
            }
            else if (ColumnName == "Learn")
            {
                return SNew(SBox).Padding(CellPadding)
                    [
                        SNew(SButton)
                            .Text(FText::FromString("Learn"))
                            .OnClicked_Lambda([W = OwnerWindow, R = RowItem]()
                                {
                                    if (auto P = W.Pin()) P->OnLearnClicked(R);
                                    return FReply::Handled();
                                })
                    ];
            }
            else if (ColumnName == "Mapping")
            {
                return SNew(SBox).Padding(CellPadding)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([R = RowItem]()
                                {
                                    return R->BoundControlId >= 0
                                        ? FText::FromString(FString::Printf(TEXT("MIDI #%d"), R->BoundControlId))
                                        : FText::FromString("Unmapped");
                                })
                    ];
            }

            return SNew(SBox).Padding(CellPadding)
                [
                    SNew(STextBlock).Text(FText::FromString("Unknown"))
                ];
        }
    };

    return SNew(SMidiMappingRow, OwnerTable)
        .RowItem(InItem)
        .OwnerWindow(SharedThis(this));
}
