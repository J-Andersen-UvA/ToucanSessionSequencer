#pragma once
#include "CoreMinimal.h"
#include "MidiTypes.h"
#include "UObject/NoExportTypes.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMidiLearn, int32 /*ControlID*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMidiAction, FName /*ActionName*/, FMidiControlValue /*Value*/);

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
    void ArmLearnOnce();
    bool IsLearning() const { return bLearning; }
    FOnMidiLearn& OnMidiLearn() { return OnLearn; }

    // Fire when a mapped action should execute
    FOnMidiAction& OnMidiAction() { return MidiActionDelegate; }

    void BroadcastAction(FName ActionName, const FMidiControlValue& Value)
    {
        MidiActionDelegate.Broadcast(ActionName, Value);
    }

private:
    UPROPERTY()
    UMidiMappingManager* Manager;

    void TryBind();              // attempt immediate bind
    void BindAfterEngineInit();  // deferred bind

    bool bLearning = false;
    FOnMidiLearn OnLearn;

    FOnMidiAction MidiActionDelegate;

};
