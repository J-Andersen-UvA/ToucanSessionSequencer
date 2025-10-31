#include "MidiMappingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"

UMidiMappingManager* UMidiMappingManager::Get()
{
    static UMidiMappingManager* Singleton = nullptr;
    if (!Singleton)
    {
        Singleton = NewObject<UMidiMappingManager>();
        Singleton->AddToRoot();
    }
    return Singleton;
}

void UMidiMappingManager::Initialize(const FString& InDeviceName, const FString& InRigName)
{
    // Only load once per device; keeps mapping memory persistent.
    if (!Mappings.Contains(InDeviceName))
    {
        FMidiDeviceMapping& DevMap = Mappings.Add(InDeviceName);
        DevMap.RigName = InRigName;
        LoadMappings(InDeviceName, InRigName);
    }
}

void UMidiMappingManager::RegisterMapping(const FString& InDeviceName, int32 ControlID, const FMidiMappedAction& Action)
{
    FMidiDeviceMapping& DevMap = Mappings.FindOrAdd(InDeviceName);
    DevMap.ControlMappings.Add(ControlID, Action);
    SaveMappings(InDeviceName, DevMap.RigName, DevMap.ControlMappings);
}

bool UMidiMappingManager::GetMapping(const FString& InDeviceName, int32 ControlID, FMidiMappedAction& OutAction) const
{
    if (const FMidiDeviceMapping* DevMap = Mappings.Find(InDeviceName))
    {
        if (const FMidiMappedAction* Found = DevMap->ControlMappings.Find(ControlID))
        {
            OutAction = *Found;
            return true;
        }
    }
    return false;
}

void UMidiMappingManager::SaveMappings()
{
    // Save all active device maps
    for (const auto& Pair : Mappings)
    {
        const FString& DevName = Pair.Key;
        const FMidiDeviceMapping& Map = Pair.Value;
        SaveMappings(DevName, Map.RigName, Map.ControlMappings);
    }
}

void UMidiMappingManager::SaveMappings(
    const FString& InDeviceName,
    const FString& InRigName,
    const TMap<int32, FMidiMappedAction>& InMappings)
{
    const FString FilePath = GetMappingFilePath(InDeviceName, InRigName);

    TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
    for (const auto& Pair : InMappings)
    {
        TSharedPtr<FJsonObject> ActionObj = FJsonObjectConverter::UStructToJsonObject(Pair.Value);
        RootObj->SetObjectField(FString::FromInt(Pair.Key), ActionObj);
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObj, Writer);

    FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

void UMidiMappingManager::LoadMappings(const FString& InDeviceName, const FString& InRigName)
{
    FMidiDeviceMapping& DevMap = Mappings.FindOrAdd(InDeviceName);
    DevMap.RigName = InRigName;
    DevMap.ControlMappings.Empty();

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *GetMappingFilePath(InDeviceName, InRigName)))
        return;

    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
    {
        for (const auto& Pair : RootObj->Values)
        {
            const TSharedPtr<FJsonObject>* ActionObj;
            if (Pair.Value->TryGetObject(ActionObj))
            {
                FMidiMappedAction Action;
                FJsonObjectConverter::JsonObjectToUStruct(ActionObj->ToSharedRef(), &Action);
                DevMap.ControlMappings.Add(FCString::Atoi(*Pair.Key), Action);
            }
        }
    }
}

FString UMidiMappingManager::GetMappingFilePath(const FString& InDeviceName, const FString& InRigName) const
{
    FString Dir = FPaths::ProjectSavedDir() / TEXT("Config/MidiMappings");
    IFileManager::Get().MakeDirectory(*Dir, true);
    return Dir / FString::Printf(TEXT("%s_%s.json"), *InDeviceName, *InRigName);
}

bool UMidiMappingManager::RemoveMapping(const FString& InDeviceName, int32 ControlID)
{
    if (FMidiDeviceMapping* DevMap = Mappings.Find(InDeviceName))
    {
        const bool bRemoved = DevMap->ControlMappings.Remove(ControlID) > 0;
        if (bRemoved)
        {
            SaveMappings(InDeviceName, DevMap->RigName, DevMap->ControlMappings);
            return true;
        }
    }
    return false;
}

void UMidiMappingManager::RegisterOrUpdate(const FString& InDeviceName, int32 ControlID, const FMidiMappedAction& Action)
{
    RegisterMapping(InDeviceName, ControlID, Action);
}

void UMidiMappingManager::DeactivateDevice(const FString& InDeviceName)
{
    if (FMidiDeviceMapping* Existing = Mappings.Find(InDeviceName))
    {
        SaveMappings(InDeviceName, Existing->RigName, Existing->ControlMappings);
        Mappings.Remove(InDeviceName);
    }
}
