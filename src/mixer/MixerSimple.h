#pragma once
#include "mixer/IMixer.h"
#include <vector>

class MixerSimple : public IMixer {
public:
    explicit MixerSimple(const std::string& id) : id_(id) {}
    const char* id() const override { return id_.c_str(); }
    int numInputs()  const override { return 0; }
    int numOutputs() const override { return 2; }
    void registerParams(IParameterStore& ps) override {};
    void bindParams(IParameterStore& ps) override {};

    void prepare(const ProcessContext& /*ctx*/) override {}
    void process(AlchemyAudioBuffer& /*io*/, MidiBuffer& /*midi*/, const ProcessContext& /*ctx*/) override {}
    void release() override {}

    int addChannel(const ChannelDesc& d) override {
        channels_.push_back(d);
        return (int)channels_.size()-1;
    }
    void setGain(int ch, float v) override { if (inRange(ch)) channels_[ch].gain=v; }
    void setPan (int ch, float v) override { if (inRange(ch)) channels_[ch].pan =v; }
    void setSend(int ch, int /*bus*/, float amount) override { if (inRange(ch)) sends_[ch]=amount; }
private:
    bool inRange(int ch) const { return ch>=0 && ch<(int)channels_.size(); }
    std::string id_;
    std::vector<ChannelDesc> channels_;
    std::vector<float> sends_;
};