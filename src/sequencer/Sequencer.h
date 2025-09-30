
#pragma once
#include <vector>
#include <cstdint>
#include <functional>
#include "bus/EventBus.h"

// Simple step: can trigger pad or note
struct Step {
    bool active{false};
    bool isPad{true};
    int  padOrNote{0};
    float vel{1.0f};
    // microtiming offset in fraction of step (-0.5..+0.5), applied by scheduling order only (no real clock here)
    float micro{0.f};
};

struct Pattern {
    int steps{16};
    std::vector<Step> data; // length == steps
    float swing{0.f}; // 0..1
};

class Sequencer {
public:
    explicit Sequencer(std::shared_ptr<IEventBus> bus) : bus_(std::move(bus)) {}

    void setPattern(const Pattern& p) { pat_ = p; pos_ = 0; }
    const Pattern& pattern() const { return pat_; }

    // Advance one step and emit events
    void tick() {
        if (pat_.data.empty()) return;
        const Step& s = pat_.data[pos_ % pat_.steps];
        if (s.active) {
            if (s.isPad) bus_->publish(EvPadPressed{ s.padOrNote, true });
            else         bus_->publish(EvNoteOn{ s.padOrNote, s.vel });
        }
        pos_ = (pos_ + 1) % pat_.steps;
    }
private:
    std::shared_ptr<IEventBus> bus_;
    Pattern pat_;
    int pos_{0};
};
