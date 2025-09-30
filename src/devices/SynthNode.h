#pragma once
#include "devices/ISynth.h"
#include <set>
#include <iostream>
#include <map>
#include <cmath>
#include <iostream>

class SynthNode : public ISynth {
public:
    explicit SynthNode(const std::string& id) : id_(id) {}
    const char* id() const override { return id_.c_str(); }
    int numInputs()  const override { return 0; }
    int numOutputs() const override { return 2; }
    void registerParams(IParameterStore& ps) override {};
    void bindParams(IParameterStore& ps) override {};

    void prepare(const ProcessContext& /*ctx*/) override {}
    void process(AlchemyAudioBuffer& io, MidiBuffer& /*midi*/, const ProcessContext& ctx) override {
        if (io.numChannels == 0 || io.numFrames == 0) return;
        const double sr = ctx.sampleRate > 0 ? ctx.sampleRate : 48000.0;

        for (int f=0; f<io.numFrames; ++f) {
            double sample = 0.0;

            for (auto note :  active_) {
                auto &ph = phases_[note];
                double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);
                ph += 2.0 * M_PI * freq / sr;
                if (ph > 2.0*M_PI) ph -= 2.0*M_PI;
                sample += std::sin(ph) * 0.1; // quiet
            }

            float s = static_cast<float>(sample);

            for (int ch=0; ch<io.numChannels; ++ch) {
                io.channels[ch][f] += s;
            }
        }
    }
    void release() override {}
    void noteOn(int note, float vel) override {
        active_.insert(note);
        std::cout << "[SynthNode] noteOn " << note << " vel=" << vel << "\n";
    }
    void noteOff(int note) override {
        active_.erase(note);
        std::cout << "[SynthNode] noteOff " << note << "\n";
    }
    void setWave(Wave w) override { wave_ = w; }
    void setUnison(int voices, float detune) override { unison_=voices; detune_=detune; }
private:
    std::string id_;
    Wave  wave_{Wave::Saw};
    int   unison_{1};
    float detune_{0.f};
    std::set<int> active_;
    std::map<int,double> phases_;
};