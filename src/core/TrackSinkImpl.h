#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <core/audio/AudioDefs.h>

// Приёмник DRY-сигнала для треков.
// Хранит ссылки на внешние буферы per-track (их владеет AudioEngine).
class TrackSinkImpl final : public ITrackSink {
public:
    TrackSinkImpl(std::vector<std::vector<float>>& L,
                  std::vector<std::vector<float>>& R,
                  std::vector<std::uint8_t>&       dirty)
            : L_(L), R_(R), dirty_(dirty) {}

    void addDry(int trackId,
                const float* srcL,
                const float* srcR,
                int nframes) override;

private:
    std::vector<std::vector<float>>& L_;     // [track][frame]
    std::vector<std::vector<float>>& R_;     // [track][frame]
    std::vector<std::uint8_t>&       dirty_; // [track] 0/1 — ленивое нуление
};
