
#pragma once
#include <functional>
#include "core/audio/AudioDefs.h"

class AudioOut {
public:
    using Render = std::function<void(AlchemyAudioBuffer&)>;
    bool start(int sampleRate, int blockFrames, int numChannels, Render render);
    void stop();
    ~AudioOut();
private:
    void* impl_{nullptr};
};
