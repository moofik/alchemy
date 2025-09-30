#pragma once
#include "core/audio/INode.h"
enum class Wave { Saw, Square, Noise };
struct ISynth : INode {
    virtual void noteOn(int note, float vel) = 0;
    virtual void noteOff(int note) = 0;
    virtual void setWave(Wave w) = 0;
    // унисон/детюн
    virtual void setUnison(int voices, float detune) = 0;
};