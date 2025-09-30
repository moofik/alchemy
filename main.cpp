#include <iostream>
#include <thread>
#include <chrono>
#include "core/audio/AudioDefs.h"
#include "core/audio/AudioEngine.h"
#include "core/audio/GraphSimple.h"
#include "bus/EventBusSimple.h"
#include "params/ParamStoreSimple.h"
#include "devices/SamplerNode.h"
#include "devices/SynthNode.h"
#include "mixer/MixerSimple.h"
#include "sequencer/Sequencer.h"
#include "project/Project.h"
#include "utils/WavLoader.h"
#ifdef __APPLE__
#include "platform/macos/AudioOut.h"
#endif

int main() {
    // Engine setup
    AudioEngine engine;
    engine.graph  = std::make_unique<GraphSimple>();
    engine.bus    = std::make_shared<EventBusSimple>();
    engine.params = std::make_shared<ParameterStoreSimple>();

    // Create nodes
    auto sampler = std::make_shared<SamplerNode>("sampler1");
    std::string wavPath = "assets/BRUSTREET-KICK.wav"; // поменяй на свой путь

    // ЗАГРУЗКА СЕМПЛА
    auto sb = LoadWavToSampleBuffer(wavPath, false);

    if (!sb) {
        std::cerr << "Failed to load WAV: " << wavPath << std::endl;
        return 1;
    }

    SampleRegion rgn;
    rgn.start = 0;
    rgn.end   = sb->frames;
    rgn.loop  = LoopMode::None;

    sampler->loadSample(/*bank*/0, /*pad*/0, sb, rgn);
    //ЗАГРУЗКА СЕМПЛА ОКОНЧЕНА

    auto synth   = std::make_shared<SynthNode>("synth1");
    auto mixer   = std::make_shared<MixerSimple>("mixer");


    engine.graph->addNode(sampler);
    engine.graph->addNode(synth);
    engine.graph->addNode(mixer);
    std::cout << "\nNODES ADDED";
    // Prepare
    ProcessContext ctx{};
    ctx.sampleRate = 48000.0;
    ctx.blockSize  = 512;
    ctx.tempoBpm   = 120.0;
    ctx.playing    = true;
    engine.prepare(ctx);

    // Wire EventBus to respond to Sequencer events (very simple demo)
    engine.bus->subscribe([&](const Event& e){
        if (std::holds_alternative<EvPadPressed>(e)) {
            auto ev = std::get<EvPadPressed>(e);
            if (ev.on) sampler->noteOnPad(ev.pad, 1.0f);
        } else if (std::holds_alternative<EvNoteOn>(e)) {
            auto ev = std::get<EvNoteOn>(e);
            synth->noteOn(ev.note, ev.vel);
        } else if (std::holds_alternative<EvNoteOff>(e)) {
            auto ev = std::get<EvNoteOff>(e);
            synth->noteOff(ev.note);
        }
    });
    engine.bus->commit();


    // Sequencer + simple pattern
    Sequencer seq(engine.bus);
    Pattern pat;
    pat.steps = 16;
    pat.data.resize(pat.steps);

    for (int i=0;i<pat.steps;i++) {
        pat.data[i].active = (i % 4 == 0); // four-on-the-floor
        pat.data[i].isPad = true;
        pat.data[i].padOrNote = 0; // pad 0
        pat.data[i].vel = 0.9f;
    }

    seq.setPattern(pat);

    // Save project
    Project prj;
    prj.pattern = pat;
    prj.save("alchemy_project.txt");

    
    // Start audio on macOS: process audio in callback
    const double bpm = 120.0;
    ctx.tempoBpm = bpm;

#ifdef __APPLE__
    AudioOut audio;
    const int kSampleRate = 48000;
    const int kBlock      = 512;
    const int kNumChans   = 2;

    // шаг = шестнадцатая  (для 4/4): samplesPerStep = sr * (60 / (bpm*4))
    const int samplesPerStep = (int)std::round(kSampleRate * (60.0 / (bpm * 4.0)));
    int stepAccumulator = 0;

    audio.start(kSampleRate, kBlock, kNumChans, [&](AlchemyAudioBuffer& buf){
        // 1) тикаем секвенсор по накопителю сэмплов
        stepAccumulator += buf.numFrames;
        while (stepAccumulator >= samplesPerStep) {
            seq.tick();                 // публикует EvPadPressed/EvNoteOn
            stepAccumulator -= samplesPerStep;
        }

        // 2) аудио процессинг блока
        MidiBuffer midi;
        ProcessContext c = ctx;
        c.blockSize  = buf.numFrames;
        c.sampleRate = kSampleRate;

        engine.process(buf, midi, c);
    });

    // держим процесс живым (без потоков, без ввода)
    for(;;) std::this_thread::sleep_for(std::chrono::seconds(1));
#else
    // Если НЕ macOS — можно оставить оффлайн тест (не звучит, но проверяет процессинг)
    float ch0[512] = {0};
    float ch1[512] = {0};
    float* outChans[2] = { ch0, ch1 };
    AlchemyAudioBuffer buf{ outChans, 2, 512 };
    MidiBuffer  midi{};

    for (int i=0;i<16;i++) {
        seq.tick();
        engine.process(buf, midi, ctx);
    }
#endif
}
