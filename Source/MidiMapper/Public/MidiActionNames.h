#pragma once

#include "CoreMinimal.h"

struct FMidiActionDef
{
	FName Id;
	FString Label;
	FString Type; // e.g. "Trigger", "Float"
};

namespace FMidiActionNames
{
    // Session control
    static const FName Queue_LoadNext = TEXT("Queue.LoadNext");
    static const FName Queue_BakeSave = TEXT("Queue.BakeSave");

    // Sequencer stepping
    static const FName Sequencer_StepSpeed1 = TEXT("Sequencer.StepSpeed1");
    // static const FName Sequencer_StepSpeed5 = TEXT("Sequencer.StepSpeed5"); this is the default speed, no need to map
    static const FName Sequencer_StepSpeed10 = TEXT("Sequencer.StepSpeed10");

    // Rig keying
    static const FName Rig_KeyAll = TEXT("Rig.KeyAll");
    static const FName Rig_ZeroAll = TEXT("Rig.ZeroAll");

    // Registry helper
	inline const TArray<FMidiActionDef>& GetAll()
	{
		static const TArray<FMidiActionDef> All = {
			{ Queue_LoadNext, TEXT("Queue Load Next"), TEXT("Trigger") },
			{ Queue_BakeSave, TEXT("Queue Bake & Save"), TEXT("Trigger") },
			{ Sequencer_StepSpeed1, TEXT("Sequencer Step +1"), TEXT("Trigger") },
			{ Sequencer_StepSpeed10,TEXT("Sequencer Step +10"), TEXT("Trigger") },
			{ Rig_KeyAll, TEXT("Rig Key All"), TEXT("Trigger") },
			{ Rig_ZeroAll, TEXT("Rig Zero All"), TEXT("Trigger") },
		};
		return All;
	}
}
