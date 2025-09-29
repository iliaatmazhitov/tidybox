//
// Created by Ilya Atmazhitov on 29.09.2025.
//

#include <iostream>
#include <string>
#include <vector>
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <limits.h>

using namespace std;

struct FSEvent {
    uint64_t identifier;
    string path;
    FSEventStreamEventFlags flags;

    FSEvent(uint64_t id, const string& p, FSEventStreamEventFlags f)
            : identifier(id), path(p), flags(f) {}
};

class FSEventsService {
public:
    FSEventStreamRef stream = nullptr;

    void handleEvent(const FSEvent& event) {
        try {
            cout << "Event ID: " << event.identifier
                      << ", Path: " << event.path
                      << ", Flags: " << event.flags << endl;
        } catch (const exception& e) {
            cerr << "Error in handleEvent: " << e.what() << endl;
        }
    }

    void stop() {
        if (stream) {
            FSEventStreamStop(stream);
            FSEventStreamInvalidate(stream);
            FSEventStreamRelease(stream);
            stream = nullptr;
        }
    }

    ~FSEventsService() {
        stop();
    }
};

//ConstFSEventStreamRef streamRef, void * __nullable clientCallBackInfo, size_t numEvents, void *eventPaths,
//const FSEventStreamEventFlags  * _Nonnull eventFlags, const FSEventStreamEventId * _Nonnull eventIds);

void eventCallback(
        ConstFSEventStreamRef streamRef, void* contextInfo, size_t numEvents, void* eventPaths,
        const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[]) {
    if (!contextInfo) {
        cerr << "Error: contextInfo is NULL!" << endl;
        return;
    }

    auto* mySelf = static_cast<FSEventsService*>(contextInfo);

    // Handle eventPaths as CFArrayRef since we're using kFSEventStreamCreateFlagUseCFTypes
    CFArrayRef cfArray = static_cast<CFArrayRef>(eventPaths);

    vector<FSEvent> events;
    for (size_t i = 0; i < numEvents; i++) {
        CFStringRef cfPath = (CFStringRef)CFArrayGetValueAtIndex(cfArray, i);
        char pathBuffer[PATH_MAX];
        if (CFStringGetCString(cfPath, pathBuffer, PATH_MAX, kCFStringEncodingUTF8)) {
            events.emplace_back(eventIds[i], pathBuffer, eventFlags[i]);
        }
    }

    for (const auto& event : events) {
        mySelf->handleEvent(event);
    }
}

// Events monitoring:
void start(FSEventsService& service, const vector<string>& pathsToWatch) {
    // Create a mutable array to store our paths as CFStringRefs
    CFMutableArrayRef cfPaths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    for (const auto& path : pathsToWatch) {
        CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
        CFArrayAppendValue(cfPaths, cfPath);
        CFRelease(cfPath);
    }

    FSEventStreamContext context = {0, &service, nullptr, nullptr, nullptr};
    // Version: 0; info: this, retain: NULL; release: NULL, copyDescription: NULL

    // Flags for FSEventStream
    FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagFileEvents;

    FSEventStreamRef stream = FSEventStreamCreate(
            kCFAllocatorDefault,
            &eventCallback,
            &context,
            cfPaths,
            kFSEventStreamEventIdSinceNow,
            0,
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

    if (!stream) {
        cerr << "Failed to create FSEventStream!" << endl;
        return;
    }

    // Store the stream in the service object
    service.stream = stream;

    dispatch_queue_t queue = dispatch_queue_create("com.example.fsevents", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(stream, queue);

    if (!FSEventStreamStart(stream)) {
        cerr << "Failed to start FSEventStream!" << endl;
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        dispatch_release(queue);
        return;
    }

    cout << "Monitoring paths:" << endl;
    for (const auto& path : pathsToWatch) {
        cout << " - " << path << endl;
    }

    cout << "Starting CFRunLoop..." << endl;
    CFRunLoopRun();

    dispatch_release(queue);
}

int main() {
    FSEventsService service;
    vector<string> pathsToWatch = {"/Users/ilya/Downloads", "/Users/ilya/Desktop"};

    cout << "Initializing FSEventsService..." << endl;

    start(service, pathsToWatch);

    return 0;
}