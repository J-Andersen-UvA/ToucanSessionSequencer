#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ToucanBakedAnimMetadata.generated.h"

class UAnimSequence;

UCLASS(BlueprintType)
class TOUCANSESSIONSEQUENCER_API UToucanBakedAnimMetadata : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Toucan")
    int32 fps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Toucan")
    int32 startTrimFrame = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Toucan")
    int32 endTrimFrame = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Toucan")
    TSoftObjectPtr<UAnimSequence> bakedAnim;
};