#include "core/TrackSinkImpl.h"
#include "core/Track.h"
#include <algorithm> // std::fill
#include <iostream> // std::fill

void TrackSinkImpl::addDry(TrackId trackId,
                           const float* srcL,
                           const float* srcR,
                           int nframes)
{
    if (trackId < 0 || trackId >= static_cast<int>(L_.size())) return;
    auto& dstL = L_[static_cast<std::size_t>(trackId)];
    auto& dstR = R_[static_cast<std::size_t>(trackId)];

    // Защита на размер (ожидается, что AudioEngine гарантирует правильный blockSize)
    if (dstL.size() < static_cast<std::size_t>(nframes) ||
        dstR.size() < static_cast<std::size_t>(nframes)) return;

    // Первый вклад за блок → обнулить только этот трек
    if (!dirty_[static_cast<std::size_t>(trackId)]) {
        std::fill(dstL.begin(), dstL.begin() + nframes, 0.f);
        std::fill(dstR.begin(), dstR.begin() + nframes, 0.f);
        dirty_[static_cast<std::size_t>(trackId)] = 1;
    }

    // Накопление dry
    for (int i = 0; i < nframes; ++i) {
        dstL[static_cast<std::size_t>(i)] += srcL[i];
        dstR[static_cast<std::size_t>(i)] += srcR[i];
    }
}
