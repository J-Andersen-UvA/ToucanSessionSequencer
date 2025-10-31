#include "MidiActionNames.h"
#include "MidiTypes.h"
#include "UObject/Object.h"
#include "MidiActionExecutor.h"
#include "EditingSessionSequencerHelper.h"

void UMidiActionExecutor::ExecuteMappedAction(FName ActionName, const FMidiControlValue& Value)
{
    using namespace FMidiActionNames;
    using Helper = FEditingSessionSequencerHelper;

    //if (ActionName == Queue_LoadNext) Helper::LoadNextAnimation(nullptr, nullptr);
    if (ActionName == Queue_BakeSave) Helper::BakeAndSave();
    else if (ActionName == Sequencer_StepSpeed1) Helper::StepFrames(1);
    else if (ActionName == Sequencer_StepSpeed10) Helper::StepFrames(10);
    else if (ActionName == Rig_KeyAll) Helper::KeyAllControls();
    else if (ActionName == Rig_ZeroAll) Helper::KeyZeroAll();
}
