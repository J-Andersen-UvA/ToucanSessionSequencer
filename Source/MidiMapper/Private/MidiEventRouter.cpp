#include "MidiEventRouter.h"
#include "UnrealMidiSubsystem.h"
#include "MidiTypes.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

void UMidiEventRouter::Init(UMidiMappingManager* InManager)
{
    Manager = InManager;
    UE_LOG(LogTemp, Log, TEXT("MidiEventRouter initialized"));
    TryBind();
}

void UMidiEventRouter::TryBind()
{
    if (GEngine)
    {
        if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
        {
            Midi->OnMidiValue.RemoveDynamic(this, &UMidiEventRouter::OnMidiValueReceived);
            Midi->OnMidiValue.AddDynamic(this, &UMidiEventRouter::OnMidiValueReceived);
            UE_LOG(LogTemp, Warning, TEXT("MidiEventRouter: bound to UnrealMidiSubsystem"));
            return;
        }
    }

    FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UMidiEventRouter::BindAfterEngineInit);
}

void UMidiEventRouter::BindAfterEngineInit()
{
    FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
    TryBind();
}

void UMidiEventRouter::ArmLearnOnce(const FString& InDeviceName)
{
    bLearning = true;
    ActiveLearningDevice = InDeviceName;
}

void UMidiEventRouter::OnMidiValueReceived(const FMidiControlValue& Value)
{
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("MidiEventRouter: Manager pointer is null!"));
        return;
    }

    // Parse: IN:<DeviceName>:<TYPE>:<Chan>:<Num>
    int32 ControlID = -1;
    {
        TArray<FString> Parts;
        Value.Id.ParseIntoArray(Parts, TEXT(":"), true);
        if (Parts.Num() >= 5)
            ControlID = FCString::Atoi(*Parts[4]);
    }
    if (ControlID < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("MidiEventRouter: couldn't parse control id from %s"), *Value.Id);
        return;
    }

    FString DeviceName;
    if (!UUnrealMidiSubsystem::ParseDeviceFromId(Value.Id, DeviceName))
        return;

    // --- learning path ---
    if (bLearning)
    {
        if (!DeviceName.Equals(ActiveLearningDevice, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogTemp, Warning, TEXT("Ignoring learn input from %s, waiting for %s"), *DeviceName, *ActiveLearningDevice);
            return; // wrong device
        }

        bLearning = false;
        LastLearnedControl = ControlID;
        OnLearn.Broadcast(DeviceName, ControlID);
        bSuppressNext = true;     // prevent the immediate next event from firing
        return;
    }

    // --- suppression path ---
    if (bSuppressNext)
    {
        if (ControlID == LastLearnedControl)
        {
            bSuppressNext = false;
            return; // skip one duplicate event
        }
        bSuppressNext = false;
    }

    int32 DotPos;
    if (DeviceName.FindChar('.', DotPos))
    {
        FString Left = DeviceName.Left(DotPos);
        FString Right = DeviceName.Mid(DotPos + 1);
        if (Left.Equals(Right, ESearchCase::IgnoreCase))
            DeviceName = Left;
    }

    // --- normal mapped execution ---
    FMidiMappedAction Action;
    if (Manager->GetMapping(DeviceName, ControlID, Action))
    {
        UE_LOG(LogTemp, Warning, TEXT("MIDI control %d (%s) from %s triggered %s (%s:%s) value=%.3f"),
            ControlID,
            *Value.Label,
            *Value.Id,
            *Action.ActionName.ToString(),
            *Action.TargetControl.ToString(),
            *Action.Modus.ToString(),
            Value.Value);
        // When SequencerControlSubsystem is implemented:
        // SequencerControlSubsystem::ExecuteMappedAction(Action, NumericValue);
    }
}
