#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MidiMappingManager.generated.h"

USTRUCT(BlueprintType)
struct FMidiMappedAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MIDI")
    FName ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MIDI")
    FName TargetControl;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MIDI")
    FName Modus;
};

UCLASS()
class MIDIMAPPER_API UMidiMappingManager : public UObject
{
    GENERATED_BODY()

public:
    static UMidiMappingManager* Get();

    void Initialize(const FString& InDeviceName, const FString& InRigName);
    void RegisterMapping(int32 ControlID, const FMidiMappedAction& Action);
    bool GetMapping(int32 ControlID, FMidiMappedAction& OutAction) const;
    void SaveMappings();
    void LoadMappings();

private:
    FString DeviceName;
    FString RigName;
    FString MappingFilePath;

    UPROPERTY()
    TMap<int32, FMidiMappedAction> ControlMappings;

    FString GetMappingFilePath() const;
};
