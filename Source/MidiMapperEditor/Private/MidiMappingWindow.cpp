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
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigControlHierarchy.h"
#include "Rigs/RigHierarchyContainer.h"
#include "Units/RigUnitContext.h"
#include "MidiActionNames.h"

#define GRouter FMidiMapperModule::GetRouter()

static FString RigControlTypeToString(ERigControlType Type)
{
    switch (Type)
    {
    case ERigControlType::Float: return TEXT("Float");
    case ERigControlType::Bool: return TEXT("Bool");
    case ERigControlType::Integer: return TEXT("Int");
    case ERigControlType::Vector2D: return TEXT("Vec2D");
    case ERigControlType::Position: return TEXT("Position");
    case ERigControlType::Rotator: return TEXT("Rotator");
    case ERigControlType::Scale: return TEXT("Scale");
    case ERigControlType::Transform: return TEXT("Transform");
    case ERigControlType::TransformNoScale: return TEXT("TransformNoScale");
    default: return TEXT("Other");
    }
}

UControlRig* LoadControlRigFromPath(const FString& RigPath)
{
    if (RigPath.IsEmpty())
        return nullptr;

    UObject* RigAsset = FSoftObjectPath(RigPath).TryLoad();
    if (!RigAsset)
        return nullptr;

    // Case 1: Blueprint-based ControlRig asset
    if (UBlueprint* RigBP = Cast<UBlueprint>(RigAsset))
    {
        if (UClass* GenClass = RigBP->GeneratedClass)
        {
            return Cast<UControlRig>(GenClass->GetDefaultObject());
        }
    }

    // Case 2: Direct ControlRig asset
    if (UControlRig* DirectRig = Cast<UControlRig>(RigAsset))
    {
        return DirectRig;
    }

    // Case 3: Sometimes it's a ControlRigBlueprintGeneratedClass
    if (UClass* RigClass = Cast<UClass>(RigAsset))
    {
        return Cast<UControlRig>(RigClass->GetDefaultObject());
    }

    return nullptr;
}

void SMidiMappingWindow::PopulateFromRig(UControlRig* ControlRig)
{
    if (!ControlRig)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MidiMapping] PopulateFromRig: ControlRig is null"));
        return;
    }

    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MidiMapping] PopulateFromRig: No hierarchy found"));
        return;
    }

    const TArray<FRigControlElement*> Controls = Hierarchy->GetControls();
    for (const FRigControlElement* CtrlElem : Controls)
    {
        if (!CtrlElem)
            continue;

        const FName ControlName = CtrlElem->GetFName();
        const ERigControlType ControlType = CtrlElem->Settings.ControlType;

        auto Row = MakeShared<FControlRow>();
        Row->ActionName = ControlName.ToString();
        Row->TargetControl = FString::Printf(TEXT("Rig.%s"), *Row->ActionName);
        Row->Modus = RigControlTypeToString(ControlType);
        Row->BoundControlId = -1;

        Rows.Add(Row);
    }

    UE_LOG(LogTemp, Display, TEXT("[MidiMapping] Populated %d controls from rig '%s'"),
        Rows.Num(), *ControlRig->GetName());

    if (MappingListView.IsValid())
    {
        MappingListView->RequestListRefresh();
    }
}

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
    Rows.Empty();

    for (const FMidiActionDef& Def : FMidiActionNames::GetAll())
    {
        auto Row = MakeShared<FControlRow>();
        Row->ActionName = Def.Label;
        Row->TargetControl = Def.Id.ToString();
        Row->Modus = Def.Type;
        Row->BoundControlId = -1;
        Rows.Add(Row);
    }

    FString RigPath;
    GConfig->GetString(TEXT("ToucanEditingSession"), TEXT("LastSelectedRig"), RigPath, GEditorPerProjectIni);

    if (RigPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MidiMapping] No rig path found in config"));
        ActiveRigName = TEXT("None");
        return;
    }

    UControlRig* ControlRig = LoadControlRigFromPath(RigPath);
    if (ControlRig)
    {
        ActiveRigName = ControlRig->GetName();
        UE_LOG(LogTemp, Display, TEXT("[MidiMapping] Loaded ControlRig: %s"), *ActiveRigName);
        PopulateFromRig(ControlRig);
    }
    else
    {
        ActiveRigName = TEXT("Invalid");
        UE_LOG(LogTemp, Warning, TEXT("[MidiMapping] Could not resolve rig from: %s"), *RigPath);
    }
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
