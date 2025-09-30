// src/tracks/TrackManager.cpp
#include "core/TrackManager.h"
#include "core/Track.h"
#include "core/TrackCommands.h"
#include "devices/IFXFactory.h"
#include <cassert>
#include <utility>   // std::move

TrackManager::TrackManager(int numTracks, int blockSize, IFxRegistry* registry)
        : sr_(48000)
        , bs_(blockSize)
        , registry_(registry)
{
    if (numTracks < 0) numTracks = 0;
    tracks_.reserve(static_cast<size_t>(numTracks));
    for (int i = 0; i < numTracks; ++i) {
        tracks_.emplace_back(i);
    }
}

void TrackManager::prepare(int sampleRate, int blockSize)
{
    sr_ = sampleRate;
    bs_ = blockSize;
    for (auto& t : tracks_) {
        t.prepare(sr_, bs_);
    }
}

void TrackManager::reset()
{
    for (auto& t : tracks_) {
        t.reset();
    }
}

void TrackManager::bindBuses(float** L, float** R, int frames)
{
    // Ожидается, что L/R указывают на массивы длиной >= numTracks(),
    // а каждый элемент — на буфер длиной >= frames.
    const int n = numTracks();
    for (int i = 0; i < n; ++i) {
        TrackBus bus{
                L ? L[i] : nullptr,
                R ? R[i] : nullptr,
                frames
        };
        tracks_[static_cast<size_t>(i)].bindBus(bus);
    }
}

void TrackManager::processAll()
{
    // RT-путь: просто прогоняем цепочки всех треков (in-place на привязанных буферах)
    for (auto& t : tracks_) {
        t.processChain();
    }
}

bool TrackManager::apply(const TrackCommand& cmd)
{
    // ВНИМАНИЕ: НЕ RT. Вызывается между аудиоблоками.
    if (std::holds_alternative<CmdAddFx>(cmd)) {
        const auto& c = std::get<CmdAddFx>(cmd);
        if (c.track < 0 || c.track >= numTracks() || !registry_) return false;

        auto fx = registry_->create(c.type);
        if (!fx) return false;

        if (c.index < 0) {
            return tracks_[static_cast<size_t>(c.track)]
                    .addEffect(std::move(fx));
        } else {
            return tracks_[static_cast<size_t>(c.track)]
                    .insertEffect(static_cast<size_t>(c.index), std::move(fx));
        }
    }

    if (std::holds_alternative<CmdRemoveFx>(cmd)) {
        const auto& c = std::get<CmdRemoveFx>(cmd);
        if (c.track < 0 || c.track >= numTracks()) return false;
        return tracks_[static_cast<size_t>(c.track)]
                .removeEffect(static_cast<size_t>(c.index));
    }

    if (std::holds_alternative<CmdMoveFx>(cmd)) {
        const auto& c = std::get<CmdMoveFx>(cmd);
        if (c.track < 0 || c.track >= numTracks()) return false;
        return tracks_[static_cast<size_t>(c.track)]
                .moveEffect(static_cast<size_t>(c.from), static_cast<size_t>(c.to));
    }

    if (std::holds_alternative<CmdSetFxParam>(cmd)) {
        const auto& c = std::get<CmdSetFxParam>(cmd);
        if (c.track < 0 || c.track >= numTracks()) return false;
        return tracks_[static_cast<size_t>(c.track)]
                .setFxParam(static_cast<size_t>(c.index), c.paramId, c.value);
    }

    return false; // неизвестная команда
}
