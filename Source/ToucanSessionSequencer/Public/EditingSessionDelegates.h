#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRigChanged, const FString&);

// Export the symbol so other modules (like MidiMapper) can link to it.
TOUCANSESSIONSEQUENCER_API extern FOnRigChanged GOnRigChanged;
