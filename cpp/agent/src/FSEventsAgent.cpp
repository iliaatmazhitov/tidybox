//
// Created by Ilya Atmazhitov on 02.10.2025.
//

#include <iostream>
#include <CoreFoundation/CoreFoundation.h>
#include <stdexcept>
#include <filesystem>
#include "../include/FSEventsAgent.h"

FSEventsAgent::FSEventsAgent(const vector<string>& paths, double latency) : paths_(paths), latency_(latency) {
    if (paths.empty()) {
        throw invalid_argument("FSEventsAgent: paths list cannot be empty");
    }

    for (const auto& path : paths_) {
        if (!filesystem::exists(path)) {
            cerr << "[WARN] Path does not exist: " << path << endl;
        }
    }
}

FSEventsAgent::~FSEventsAgent() {
    stop();
}

bool FSEventsAgent::start(EventCallback cb) {
    if (stream_) {
        cerr << "[FSEventsAgent] already started, ignoring second start()." << endl;
        return false;
    }

    if (!cb) {
        cerr << "[FSEventsAgent] empty callback passed to start()." << endl;
        return false;
    }

    callback_ = move(cb);

    // Create a mutable array to store our paths as CFStringRefs
    CFMutableArrayRef cfPaths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    for (const auto& path : paths_) {
        CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
        CFArrayAppendValue(cfPaths, cfPath);
        CFRelease(cfPath);
    }

    FSEventStreamContext context = {0, this, nullptr, nullptr, nullptr};
    // Version: 0; info: this, retain: NULL; release: NULL, copyDescription: NULL

    // Flags for FSEventStream
    FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagFileEvents;

    stream_ = FSEventStreamCreate(
            kCFAllocatorDefault,
            &FSEventsAgent::StaticCallback,
            &context,
            cfPaths,
            kFSEventStreamEventIdSinceNow,
            latency_,
            flags
    );

////    CFAllocatorRef __nullable  allocator,
////    FSEventStreamCallback      callback,
////    FSEventStreamContext * __nullable context,
////            CFArrayRef                 pathsToWatch,
////            FSEventStreamEventId       sinceWhen,
////            CFTimeInterval             latency,
////            FSEventStreamCreateFlags   flags)

    CFRelease(cfPaths);

    if (!stream_) {
        cerr << "Failed to create FSEventStream!" << endl;
        return false;
    }

    queue_ = dispatch_queue_create("com.example.fsevents", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(stream_, queue_);

    if (!FSEventStreamStart(stream_)) {
        cerr << "Failed to start FSEventStream!" << endl;
        FSEventStreamInvalidate(stream_);
        FSEventStreamRelease(stream_);
        stream_ = nullptr;
        dispatch_release(queue_);
        queue_ = nullptr;
        return false;
    }

    cout << "Started monitoring paths:" << endl;
    for (const auto& path : paths_) {
        cout << " - " << path << endl;
    }
    return true;
}

void FSEventsAgent::stop() {
    if (!stream_) return;
    FSEventStreamStop(stream_);
    FSEventStreamSetDispatchQueue(stream_, NULL);
    FSEventStreamInvalidate(stream_);
    FSEventStreamRelease(stream_);
    stream_ = nullptr;
    dispatch_release(queue_);
    queue_ = nullptr;
}

void FSEventsAgent::StaticCallback(
        ConstFSEventStreamRef /*streamRef*/,
        void* clientCallBackInfo,
        size_t numEvents,
        void* eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]
) {
    if (!clientCallBackInfo) return;
    auto* self = static_cast<FSEventsAgent*>(clientCallBackInfo);
    self->InstanceCallback(numEvents, eventPaths, eventFlags, eventIds);
}

void FSEventsAgent::InstanceCallback(
        size_t numEvents,
        void* eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]
) {
    vector<FSEvent> events;
    CFArrayRef cfArray = static_cast<CFArrayRef>(eventPaths);

    for (size_t i = 0; i < numEvents; i++) {
        CFStringRef cfPath = (CFStringRef)CFArrayGetValueAtIndex(cfArray, i);
        char pathBuffer[PATH_MAX];
        if (CFStringGetCString(cfPath, pathBuffer, PATH_MAX, kCFStringEncodingUTF8)) {
            events.emplace_back(eventIds[i], pathBuffer, eventFlags[i]);
        } else {
            cerr << "[WARN] failed to convert path at index " << i << endl;
        }
    }

    if (callback_) {
        callback_(events);
    }
}