#pragma once
#include "AudioDefs.h"

struct IAudioProcessor {
    virtual ~IAudioProcessor() = default;
    virtual void prepare(const ProcessContext& ctx) = 0;
    virtual void process(AlchemyAudioBuffer& io, MidiBuffer& midi, const ProcessContext& ctx) = 0;
    virtual void release() = 0;
};