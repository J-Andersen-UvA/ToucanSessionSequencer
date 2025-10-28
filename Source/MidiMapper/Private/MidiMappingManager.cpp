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
    DeviceName = InDeviceName;
    RigName = InRigName;
    MappingFilePath = GetMappingFilePath();
    LoadMappings();
}

void UMidiMappingManager::RegisterMapping(int32 ControlID, const FMidiMappedAction& Action)
{
    ControlMappings.Add(ControlID, Action);
    SaveMappings();
}

bool UMidiMappingManager::GetMapping(int32 ControlID, FMidiMappedAction& OutAction) const
{
    if (const FMidiMappedAction* Found = ControlMappings.Find(ControlID))
    {
        OutAction = *Found;
        return true;
    }
    return false;
}

void UMidiMappingManager::SaveMappings()
{
    TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

    for (const auto& Pair : ControlMappings)
    {
        TSharedPtr<FJsonObject> ActionObj = FJsonObjectConverter::UStructToJsonObject(Pair.Value);
        RootObj->SetObjectField(FString::FromInt(Pair.Key), ActionObj);
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObj, Writer);

    FFileHelper::SaveStringToFile(OutputString, *MappingFilePath);
}

void UMidiMappingManager::LoadMappings()
{
    ControlMappings.Empty();

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *GetMappingFilePath()))
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
                ControlMappings.Add(FCString::Atoi(*Pair.Key), Action);
            }
        }
    }
}

FString UMidiMappingManager::GetMappingFilePath() const
{
    FString Dir = FPaths::ProjectSavedDir() / TEXT("Config/MidiMappings");
    IFileManager::Get().MakeDirectory(*Dir, true);
    return Dir / FString::Printf(TEXT("%s_%s.json"), *DeviceName, *RigName);
}
