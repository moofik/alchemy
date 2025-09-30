#include "core/audio/AudioEngine.h"
#include "core/audio/GraphSimple.h"
#include "devices/SamplerNode.h"
#include "utils/TrackPath.h"
#include <iostream>

void AudioEngine::prepare(const ProcessContext& ctx) {
    // 1) треки: реши количество (напр. банки*пэды сэмплера, либо из конфига проекта)
    tracksCount_ = 16 * 4; // пример; вынеси в конфиг
    if (!trackMgr_) {
        // передай реальный IFxRegistry*, когда будет готов (можешь хранить его в AudioEngine)
        trackMgr_ = std::make_unique<TrackManager>(tracksCount_, ctx.blockSize, /*registry*/nullptr);
    }
    trackMgr_->prepare((int) ctx.sampleRate, ctx.blockSize);

    // 2) per-track буферы
    ensureTrackBuffers(tracksCount_, ctx.blockSize);

    // 3) граф + шина событий остаются как были
    graph->forEachNode([&](INode& n){ n.registerParams(*params); });
    params->commitParams();
    graph->forEachNode([&](INode& n){ n.bindParams(*params); });

    if (graph) graph->prepare(ctx);

    if (bus) {
        bus->subscribe([this](const Event& e){ this->handleEvent(e); });
    }
}

void AudioEngine::process(AlchemyAudioBuffer& io, MidiBuffer& midi, const ProcessContext& ctx) {
    // 0) применить команды для треков (между блоками)
    drainTrackCommands();

    // 1) синхронизировать размеры на случай изменений
    if ((int)trackBufL_.size() != tracksCount_ || (int)trackBufL_[0].size() != ctx.blockSize) {
        ensureTrackBuffers(tracksCount_, ctx.blockSize);
        trackMgr_->prepare((int) ctx.sampleRate, ctx.blockSize);
    }

    // 2) подготовить указатели и dirty-флаги на блок
    trackPtrsL_.resize(tracksCount_);
    trackPtrsR_.resize(tracksCount_);
    for (int t=0; t<tracksCount_; ++t) {
        trackPtrsL_[t] = trackBufL_[t].data();
        trackPtrsR_[t] = trackBufR_[t].data();
        trackDirty_[t] = 0;
    }

    // 3) привязать буферы к TrackManager (они же будут обработаны in-place)
    trackMgr_->bindBuses(trackPtrsL_.data(), trackPtrsR_.data(), ctx.blockSize);

    // 4) прогнать граф источников, но с trackSink в контексте
    ProcessContext ctx2 = ctx;
    ctx2.tracks = &trackSink_;      // <-- вот куда Sampler/Synth будут писать DRY

    // очень важно: контекст всё ещё несёт params и bus (как и раньше)
    if (graph) graph->process(io, midi, ctx2);

    // 5) пер-трековые вставки (processChain по каждому треку)
    //trackMgr_->processAll();

    // 6) (опц.) глобальные aux FX-ноды — если есть и работают как ноды графа, граф их уже прогонит
    //    либо сделай отдельный проход здесь

    // 7) суммирование обработанных треков в мастер (io)
    float* outL = io.channels[0];
    float* outR = io.channels[1];
    const int n = ctx.blockSize;
    std::fill(outL, outL+n, 0.f);
    std::fill(outR, outR+n, 0.f);

    for (int t=0; t<tracksCount_; ++t) {
        if (!trackDirty_[t]) {
            continue;
        }; // этот трек молчал

        const float* srcL = trackBufL_[t].data();
        const float* srcR = trackBufR_[t].data();
        for (int i=0; i<n; ++i) { outL[i] += srcL[i]; outR[i] += srcR[i]; }
    }


    // 8) (опц.) мастер-цепочка здесь же или как отдельная нода в graph
}

void AudioEngine::handleEvent(const Event& e) {
    // как и раньше — твои реакции на транспорт/пэды/загрузку сэмплов и т.п.
    // если часть событий порождает TrackCommand — просто вызови pushTrackCommand(...)
}

void AudioEngine::ensureTrackBuffers(int numTracks, int blockSize) {
    trackBufL_.resize((size_t)numTracks);
    trackBufR_.resize((size_t)numTracks);
    trackDirty_.resize((size_t)numTracks);
    for (int t=0; t<numTracks; ++t) {
        trackBufL_[t].resize((size_t)blockSize);
        trackBufR_[t].resize((size_t)blockSize);
    }
}

void AudioEngine::drainTrackCommands() {
    TrackCommand cmd;
    while (trackCmdQ_.pop(cmd)) {
        if (trackMgr_) (void)trackMgr_->apply(cmd);
    }
}