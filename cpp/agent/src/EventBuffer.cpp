//
// Created by Ilya Atmazhitov on 06.10.2025.
//

#include "../include/EventBuffer.h"

EventBuffer::EventBuffer(Config cfg, FlushCallback cb): cfg_(cfg), cb_(std::move(cb)), lastFlushPoint_(Clock::now()) {
    if (!cb_) {
        throw invalid_argument("FlushCallback can't be empty");
    }

    if (cfg_.maxBatchSize == 0) {
        throw invalid_argument("maxBatchSize must be > 0");
    }
    buffer_.reserve(cfg_.maxBatchSize);
}

bool EventBuffer::pending() const {
    return !buffer_.empty();
}

bool EventBuffer::flushByTime() const {
    auto now = Clock::now();
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastFlushPoint_).count();
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

    vector<FSEvent> tmp;
    tmp.swap(buffer_);
    cb_(std::move(tmp));
    lastFlushPoint_ = Clock::now();
}

void EventBuffer::flush() {
    flushInternal();
}

void EventBuffer::add(const FSEvent &ev) {
    buffer_.push_back(ev);
    if (flushBySize()) {
        flushInternal();
        return;
    }
    if (flushByTime()) {
        flushInternal();
        return;
    }
}

void EventBuffer::addMany(const vector<FSEvent>& events) {
    for (const auto& event: events) {
        add(event);
    }
}

string serializeBatch(const string& deviceId, const vector<FSEvent>& events) {
    ostringstream json;

    json << "{ \"device_id\":\"" << deviceId << "\", \"events\":[";

    for (size_t i = 0; i < events.size(); ++i) {
        const FSEvent& event = events[i];

        json << "{ \"id\":" << event.identifier
             << ", \"path\":\"" << event.path
             << "\", \"flags\":" << event.flags << "}";

        if (i < events.size() - 1) {
            json << ",";
        }
    }

    json << "]}";

    return json.str();
}