//
// Created by Ilya Atmazhitov on 06.10.2025.
//

#include "../include/EventBuffer.h"

EventBuffer::EventBuffer(Config cfg, FlushCallback cb): cfg_(cfg), cb_(std::move(cb)), lastFlushPoint_(Clock::now()) {
    if (!cb_) {
        throw std::invalid_argument("FlushCallback can't be empty");
    }

    if (cfg_.maxBatchSize == 0) {
        throw std::invalid_argument("maxBatchSize must be > 0");
    }
    buffer_.reserve(cfg_.maxBatchSize);
}

bool EventBuffer::flushByTime() const {
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlushPoint_).count();
    return (!buffer_.empty()) && (elapsed >= cfg_.maxLatencyMs);
}

bool EventBuffer::flushBySize() const {
    return buffer_.size() >= cfg_.maxBatchSize;
}

size_t EventBuffer::size() const {
    return buffer_.size();
}

void EventBuffer::flushInternal() {
    if (buffer_.empty()) {
        return;
    }

    std::vector<FSEvent> tmp;
    tmp.swap(buffer_);
    cb_(std::move(tmp));
    lastFlushPoint_ = Clock::now();
}

void EventBuffer::flush() {
    flushInternal();
}

void EventBuffer::add(const FSEvent &ev) {
    buffer_.push_back(ev);
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlushPoint_).count();
    if (flushBySize()) {
        flushInternal();
        return;
    }
    if (flushByTime()) {
        flushInternal();
        return;
    }
}

void EventBuffer::addMany(const std::vector<FSEvent>& events) {
    for (auto event: events) {
        add(event);
    }
}

