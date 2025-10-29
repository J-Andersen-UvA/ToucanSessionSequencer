#pragma once
#include "CoreMinimal.h"
#include "MidiTypes.h"
#include "MidiActionExecutor.generated.h"

UCLASS()
class MIDIMAPPEREDITOR_API UMidiActionExecutor : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    void ExecuteMappedAction(FName ActionName, const FMidiControlValue& Value);
};
