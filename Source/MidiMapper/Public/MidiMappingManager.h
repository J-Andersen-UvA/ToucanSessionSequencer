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

USTRUCT(BlueprintType)
struct FMidiDeviceMapping
{
    GENERATED_BODY()
    FString RigName;
    TMap<int32, FMidiMappedAction> ControlMappings;
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
    void SaveMappings(const FString& InDeviceName, const FString& InRigName,
        const TMap<int32, FMidiMappedAction>& InMappings);
    void LoadMappings();
    const TMap<int32, FMidiMappedAction>& GetAll() const { return ControlMappings; }
    bool RemoveMapping(int32 ControlID);
    void RegisterOrUpdate(int32 ControlID, const FMidiMappedAction& Action);

    UFUNCTION()
    void DeactivateDevice(const FString& InDeviceName);

    FString GetMappingFilePath() const;
    FString GetMappingFilePath(const FString& InDeviceName, const FString& InRigName) const;
private:
    FString DeviceName;
    FString RigName;
    FString MappingFilePath;

    UPROPERTY()
    TMap<int32, FMidiMappedAction> ControlMappings;

    UPROPERTY()
    TMap<FString, FMidiDeviceMapping> Mappings;

};
