//
// Created by Ilya Atmazhitov on 06.10.2025.
//

#ifndef TIDYBOX_AGENT_EVENTBUFFER_H
#define TIDYBOX_AGENT_EVENTBUFFER_H

#include <vector>
#include <functional>
#include <stdexcept>
#include "FSEventsAgent.h"

class EventBuffer {
public:
    struct Config {
        size_t maxBatchSize = 50;
        uint64_t maxLatencyMs = 500;

        Config(size_t size = 50, uint64_t latency = 500):  maxBatchSize(size), maxLatencyMs(latency) {}
    };
    using FlushCallback =  std::function<void(std::vector<FSEvent>&&)>;
    void flushInternal();
    bool flushByTime() const;
    bool flushBySize() const;
    EventBuffer(Config cfg, FlushCallback cb);
    void add(const FSEvent& ev);
    void addMany(const std::vector<FSEvent>& events);
    void flush();
    size_t size() const;
private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point lastFlushPoint_;
    Config cfg_;
    FlushCallback cb_;
    std::vector<FSEvent> buffer_;
};

#endif //TIDYBOX_AGENT_EVENTBUFFER_H
