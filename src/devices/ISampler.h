#pragma once
#include "core/audio/INode.h"
struct SampleSlice { int start{}, end{}; bool loop=false; };
struct ISampler : INode {
    virtual void setSample(int pad, const float* data, int frames, int channels, double sr) = 0;
    virtual void triggerPad(int pad, float velocity) = 0;
    virtual void setSlice(int pad, const SampleSlice&) = 0;
};