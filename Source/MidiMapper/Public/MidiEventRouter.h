#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MidiMappingManager.h"
#include "MidiEventRouter.generated.h"

UCLASS()
class MIDIMAPPER_API UMidiEventRouter : public UObject
{
    GENERATED_BODY()

public:
    void Init(UMidiMappingManager* InManager);

    /** Handler for UnrealMidi's OnMidiValue */
    UFUNCTION()
    void OnMidiValueReceived(const FMidiControlValue& Value);

private:
    UPROPERTY()
    UMidiMappingManager* Manager;

    void TryBind();              // attempt immediate bind
    void BindAfterEngineInit();  // deferred bind

};
