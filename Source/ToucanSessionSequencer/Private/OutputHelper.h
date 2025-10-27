#pragma once
#include "CoreMinimal.h"

class FOutputHelper
{
public:
    static void LoadSettings();
    static void SaveSettings();

    static FString Get();
    static void Set(const FString& NewPath);

    static FString GetDefaultFolder();
    static FString EnsureDatedSubfolder();

private:
    static FString CurrentFolder;
    static constexpr const TCHAR* ConfigSection = TEXT("ToucanEditingSession");
    static constexpr const TCHAR* ConfigKey = TEXT("LastSelectedOutputFolder");
};
