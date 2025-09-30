#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ITrackSink {
    virtual ~ITrackSink() = default;
    // RT-safe: без аллокаций/локов/исключенийы
    virtual void addDry(int trackId,
                        const float* L,
                        const float* R,
                        int nframes) = 0;
};

struct AlchemyAudioBuffer {
    float** channels; // non-owning
    int numChannels;
    int numFrames;
};
struct MidiEvent { uint8_t status, data1, data2; uint32_t sampleOffset; };
using MidiBuffer = std::vector<MidiEvent>;

struct ProcessContext {
    double sampleRate{};
    int    blockSize{};   // frames per block
    double tempoBpm{};
    double transportPosBeats{};
    bool   playing{};
    ITrackSink*    tracks = nullptr;
};
