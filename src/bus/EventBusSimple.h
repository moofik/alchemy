#pragma once
#include "bus/EventBus.h"
#include <mutex>
#include <utils/SpscRing.h>

class EventBusSimple : public IEventBus {
public:
    void publishFromUI(const Event& e) override {
        spscQueue_.push(e);
    }
    void drainUIEvents() override {
        Event ev;

        while (spscQueue_.pop(ev)) {
            publish(ev);
        }
    }
    void publish(const Event& e) override {
        for (auto& h : handlers_) h(e);
    }
    void subscribe(Handler h) override {
        std::lock_guard<std::mutex> lk(mu_);
        staging_.push_back(std::move(h));
    }
    void commit() override {
        if (freeze_) {
            return;
        }
        std::lock_guard<std::mutex> lk(mu_);
        handlers_ = staging_;
        freeze_ = true;
    }
private:
    bool freeze_{false};
    std::mutex mu_;
    std::vector<Handler> handlers_;
    std::vector<Handler> staging_;
    SpscRing<Event, 1024> spscQueue_;
};