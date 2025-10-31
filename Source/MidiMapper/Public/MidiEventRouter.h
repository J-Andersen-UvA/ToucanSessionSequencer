#pragma once
#include "CoreMinimal.h"
#include "MidiTypes.h"
#include "UObject/NoExportTypes.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMidiLearnSignature, FString /*DeviceName*/, int32 /*ControlId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMidiAction, FName /*ActionName*/, FMidiControlValue /*Value*/);
DECLARE_MULTICAST_DELEGATE(FOnLearningCancelled);

UCLASS()
class MIDIMAPPER_API UMidiEventRouter : public UObject
{
    GENERATED_BODY()

public:
    void Init(UMidiMappingManager* InManager);

    /** Handler for UnrealMidi's OnMidiValue */
    UFUNCTION()
    void OnMidiValueReceived(const FMidiControlValue& Value);

    // learn API
    void ArmLearnOnce(const FString& InDeviceName);
    bool IsLearning() const { return bLearning; }
    FOnMidiLearnSignature& OnMidiLearn() { return OnLearn; }
    void CancelLearning();

    // Fire when a mapped action should execute
    FOnMidiAction& OnMidiAction() { return MidiActionDelegate; }

    void BroadcastAction(FName ActionName, const FMidiControlValue& Value)
    {
        MidiActionDelegate.Broadcast(ActionName, Value);
    }

    void TryBind();              // attempt immediate bind
    FOnLearningCancelled OnLearningCancelled;

private:
    UPROPERTY()
    UMidiMappingManager* Manager;

    void BindAfterEngineInit();  // deferred bind

    bool bLearning = false;
    bool bSuppressNext = false;
    int32 LastLearnedControl = -1;
    FOnMidiLearnSignature OnLearn;
    FString ActiveLearningDevice;

    FOnMidiAction MidiActionDelegate;

};
