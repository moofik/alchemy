#pragma once
#include <string>
#include "IAudioProcessor.h"
#include "params/Param.h"

struct INode : IAudioProcessor {
    virtual const char* id() const = 0;          // "sampler1", "synth1", "fuzz", "reverb"
    virtual int numInputs()  const = 0;          // стерео = 2
    virtual int numOutputs() const = 0;
    virtual void registerParams(IParameterStore&) = 0;
    virtual void bindParams(IParameterStore&) = 0;
};