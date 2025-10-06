//
// Created by Ilya Atmazhitov on 29.09.2025.
//

#include <iostream>
#include <unistd.h>
#include "../include/FSEventsAgent.h"
#include "../include/EventBuffer.h"

using namespace std;

string getSimpleDeviceId() {
    static string deviceId;
    if (deviceId.empty()) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            deviceId = std::string(hostname) + "-" + std::to_string(time(nullptr));
        } else {
            deviceId = "unknown-device-" + std::to_string(time(nullptr));
        }
    }
    return deviceId;
}

int main() {
    vector<string> pathsToWatch = {
            "/Users/ilya/Downloads",
            "/Users/ilya/Desktop"
    };

    std::string deviceId = getSimpleDeviceId();
    cout << "Device ID: " << deviceId << endl;

    EventBuffer buffer(
            EventBuffer::Config(5, 3000),
            [deviceId](std::vector<FSEvent>&& batch){
                cout << "=== FLUSH batch size=" << batch.size() << " ===" << endl;

                for (const auto& e : batch) {
                    cout << "  id=" << e.identifier
                         << " path=" << e.path
                         << " flags=" << e.flags
                         << endl;
                }

                cout << "=== JSON OUTPUT ===" << endl;
                std::string json = serializeBatch(deviceId, batch);
                cout << json << endl;
                cout << "===================" << endl;
            }
    );

    FSEventsAgent agent(pathsToWatch, 0.5);
    bool started = agent.start([&](const vector<FSEvent>& events){
        buffer.addMany(events);
    });

    if (!started) {
        cerr << "Failed to start FSEventsAgent" << endl;
        return 1;
    }

    cout << "Watching paths:" << endl;
    for (const auto& path : pathsToWatch) {
        cout << "  - " << path << endl;
    }
    cout << "Batch settings: maxSize=5, maxLatencyMs=3000" << endl;
    cout << "Create files in watched directories to test..." << endl;

    CFRunLoopRun();

    buffer.flush();

    return 0;
}