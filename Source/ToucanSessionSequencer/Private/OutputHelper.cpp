#include "OutputHelper.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"

FString FOutputHelper::CurrentFolder = FOutputHelper::GetDefaultFolder();

FString FOutputHelper::GetDefaultFolder()
{
    return TEXT("/Game/ToucanTemp/Output");
}

FString FOutputHelper::Get()
{
    return CurrentFolder;
}

void FOutputHelper::Set(const FString& NewPath)
{
    CurrentFolder = NewPath;
    SaveSettings();
}

void FOutputHelper::LoadSettings()
{
#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif

    FString SavedPath;
    GConfig->GetString(ConfigSection, ConfigKey, SavedPath, Ini);
    if (!SavedPath.IsEmpty())
        CurrentFolder = SavedPath;
}

void FOutputHelper::SaveSettings()
{
#if WITH_EDITOR
    const FString& Ini = GEditorPerProjectIni;
#else
    const FString& Ini = GGameIni;
#endif

    GConfig->SetString(ConfigSection, ConfigKey, *CurrentFolder, Ini);
    GConfig->Flush(false, Ini);
}

FString FOutputHelper::EnsureDatedSubfolder()
{
    FString DateString = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
    FString NewPath = CurrentFolder / DateString;
    FString NormalizedPath = NewPath.Replace(TEXT("//"), TEXT("/"));

#if WITH_EDITOR
    if (!UEditorAssetLibrary::DoesDirectoryExist(NormalizedPath))
    {
        UEditorAssetLibrary::MakeDirectory(NormalizedPath);
        UE_LOG(LogTemp, Display, TEXT("[ToucanSequencer] Created dated output folder: %s"), *NormalizedPath);
    }
#endif

    return NormalizedPath;
}
