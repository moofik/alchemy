#pragma once
#include <memory>
#include <vector>
#include <string>
#include "AudioDefs.h"
#include "INode.h"

struct IGraph {
    virtual ~IGraph() = default;
    virtual void addNode(std::shared_ptr<INode>) = 0;
    virtual void connect(const std::string& srcId, int srcChan,
                         const std::string& dstId, int dstChan) = 0; // patchbay
    virtual void prepare(const ProcessContext&) = 0;
    virtual void process(AlchemyAudioBuffer&, MidiBuffer&, const ProcessContext&) = 0;
    virtual void clear() = 0;

    [[nodiscard]] virtual std::size_t nodeCount() const = 0;
    [[nodiscard]] virtual INode&      nodeAt(std::size_t i) const = 0;

    // удобный шаблонный обход — НЕ виртуальный (обычный инлайн в хедере)
    template<class F>
    void forEachNode(F&& f) const {
        const std::size_t n = nodeCount();
        for (std::size_t i = 0; i < n; ++i) f(nodeAt(i));
    }
};
