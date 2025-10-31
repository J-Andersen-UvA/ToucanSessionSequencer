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
        UE_LOG(LogTemp, Warning, TEXT("MidiEventRouter: attempting immediate bind"));
        if (UUnrealMidiSubsystem* Midi = GEngine->GetEngineSubsystem<UUnrealMidiSubsystem>())
        {
            Midi->OnMidiValue.AddDynamic(this, &UMidiEventRouter::OnMidiValueReceived);
            UE_LOG(LogTemp, Warning, TEXT("MidiEventRouter: bound to UnrealMidiSubsystem"));
            return;
        }
    }

    FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UMidiEventRouter::BindAfterEngineInit);
    UE_LOG(LogTemp, Verbose, TEXT("MidiEventRouter: deferring bind until engine init"));
}

void UMidiEventRouter::BindAfterEngineInit()
{
    FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
    TryBind();
}

void UMidiEventRouter::ArmLearnOnce()
{
    bLearning = true;
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

    if (bLearning)
    {
        bLearning = false;
        OnLearn.Broadcast(ControlID);
        return;
    }

    FMidiMappedAction Action;
    if (Manager->GetMapping(ControlID, Action))
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
