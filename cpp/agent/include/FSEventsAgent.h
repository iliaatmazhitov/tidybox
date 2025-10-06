//
// Created by Ilya Atmazhitov on 30.09.2025.
//

#ifndef FSEVENTSSERVICE_FSEVENTSAGENT_H
#define FSEVENTSSERVICE_FSEVENTSAGENT_H

#include <string>
#include <vector>
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <functional>
#include <limits.h>

using namespace std;

struct FSEvent {
    uint64_t identifier;
    string path;
    FSEventStreamEventFlags flags;

    FSEvent(uint64_t id, const string& p, FSEventStreamEventFlags f)
            : identifier(id), path(p), flags(f) {}
};

class FSEventsAgent {
public:
    using EventCallback = function<void(const vector<FSEvent>&)>;

    FSEventsAgent(const vector<string>& paths, double latency = 0.2);
    ~FSEventsAgent();

    bool start(EventCallback cb);
    void stop();

private:
    vector<string> paths_;
    double latency_;
    EventCallback callback_;
    FSEventStreamRef stream_ = nullptr;
    dispatch_queue_t queue_ = nullptr;

    static void StaticCallback(
            ConstFSEventStreamRef streamRef,
            void* clientCallBackInfo,
            size_t numEvents,
            void* eventPaths,
            const FSEventStreamEventFlags eventFlags[],
            const FSEventStreamEventId eventIds[]
    );

    void InstanceCallback(
            size_t numEvents,
            void* eventPaths,
            const FSEventStreamEventFlags eventFlags[],
            const FSEventStreamEventId eventIds[]
    );
};

#endif //FSEVENTSSERVICE_FSEVENTSAGENT_H
