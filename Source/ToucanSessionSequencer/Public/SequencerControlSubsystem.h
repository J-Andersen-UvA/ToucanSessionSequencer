#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SequencerControlSubsystem.generated.h"

class ISequencer;
class UMovieSceneSequence;
class UControlRig;

UCLASS()
class USequencerControlSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()
public:
    static ISequencer* GetCurrentOpenSequencer();
    static UMovieSceneSequence* GetCurrentSequence();
    static int32 GetCurrentTimeInFrames();
    static void SetCurrentTimeInFrames(int32 Frame);
    static void AdvanceByFrames(int32 DeltaFrames);
    static UControlRig* GetBoundRigFromSequencer(UMovieSceneSequence* Sequence, const FString& RigName);
};
