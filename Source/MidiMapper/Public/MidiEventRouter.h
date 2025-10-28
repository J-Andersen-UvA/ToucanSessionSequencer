#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMidiLearn, int32 /*ControlID*/);

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

private:
    UPROPERTY()
    UMidiMappingManager* Manager;

    void TryBind();              // attempt immediate bind
    void BindAfterEngineInit();  // deferred bind

    bool bLearning = false;
    FOnMidiLearn OnLearn;
};
