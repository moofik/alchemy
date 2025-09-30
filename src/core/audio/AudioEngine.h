#pragma once
#include <memory>
#include "IGraph.h"
#include "AudioDefs.h"
#include "bus/EventBus.h"
#include "params/Param.h"
#include "core/TrackManager.h"
#include "core/TrackSinkImpl.h"
#include "utils/SpscRing.h"

struct AudioEngine {
    std::unique_ptr<IGraph>          graph;
    std::shared_ptr<IParameterStore> params;
    std::shared_ptr<IEventBus>       bus;

    void prepare(const ProcessContext& ctx);
    void process(AlchemyAudioBuffer& io, MidiBuffer& midi, const ProcessContext& ctx); // JUCE audio callback
    void handleEvent(const Event& e); // subscribed via bus->subscribe

    // --- новое: трековый слой ---
    void pushTrackCommand(const TrackCommand& cmd) { (void)trackCmdQ_.push(cmd); }

private:
    std::unique_ptr<TrackManager> trackMgr_;         // владелец треков/FX цепей
    int tracksCount_ = 0;

    // per-track рабочие буферы (dry → после processChain остаются там же)
    std::vector<std::vector<float>> trackBufL_;
    std::vector<std::vector<float>> trackBufR_;
    std::vector<float*>             trackPtrsL_;
    std::vector<float*>             trackPtrsR_;
    std::vector<uint8_t>            trackDirty_;     // ленивое нуление на первый вклад

    SpscRing<TrackCommand, 1024>          trackCmdQ_; // команды от UI → трековый слой


    TrackSinkImpl                   trackSink_{trackBufL_, trackBufR_, trackDirty_};

    void ensureTrackBuffers(int numTracks, int blockSize);
    void drainTrackCommands();
};
