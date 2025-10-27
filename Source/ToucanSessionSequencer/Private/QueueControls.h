#pragma once
#include "CoreMinimal.h"

class FQueueControls
{
public:
    static void AddAnimationsFromFolder();
    static void AddAnimationsByHand();
    static void RemoveAllAnimations();
    static void RemoveMarkedProcessedAnimations();

};
