#pragma once
#include <memory>
#include <unordered_map>
#include "core/audio/IGraph.h"
#include "core/audio/INode.h"

class GraphSimple : public IGraph {
public:
    [[nodiscard]] std::size_t nodeCount() const override { return nodes_.size(); }
    [[nodiscard]] INode&      nodeAt(std::size_t i) const override { return *nodes_[i]; }

    void addNode(std::shared_ptr<INode> n) override {
        nodes_.push_back(n);
        idmap_[n->id()] = n;
    }
    void connect(const std::string& srcId, int /*srcChan*/,
                 const std::string& dstId, int /*dstChan*/) override {
        // Для болванки игнорируем конкретные каналы; фиксируем порядок
        connections_.push_back({srcId, dstId});
    }
    void prepare(const ProcessContext& ctx) override {
        for (auto& n : nodes_) n->prepare(ctx);
    }
    void process(AlchemyAudioBuffer& io, MidiBuffer& midi, const ProcessContext& ctx) override {
        // Zero output buffer
        for (int ch=0; ch<io.numChannels; ++ch) {
            for (int f=0; f<io.numFrames; ++f) io.channels[ch][f] = 0.0f;
        }
        // Болванка: просто вызывает process у всех нод без реального роутинга
        for (auto& n : nodes_) n->process(io, midi, ctx);
    }
    void clear() override {
        nodes_.clear();
        idmap_.clear();
        connections_.clear();
    }
private:
    std::vector<std::shared_ptr<INode>> nodes_;
    std::unordered_map<std::string, std::shared_ptr<INode>> idmap_;
    std::vector<std::pair<std::string,std::string>> connections_;
};