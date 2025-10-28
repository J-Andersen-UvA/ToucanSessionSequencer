#pragma once
#include "Modules/ModuleManager.h"

class UMidiEventRouter;

class MIDIMAPPER_API FMidiMapperModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FMidiMapperModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FMidiMapperModule>("MidiMapper");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("MidiMapper");
    }

    static UMidiEventRouter* GetRouter();
};
