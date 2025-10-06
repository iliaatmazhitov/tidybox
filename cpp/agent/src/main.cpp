//
// Created by Ilya Atmazhitov on 29.09.2025.
//

#include <iostream>
#include "../include/FSEventsAgent.h"
#include "../include/EventBuffer.h"

using namespace std;

int main() {
    std::vector<std::string> pathsToWatch = {
            "/Users/ilya/Downloads",
            "/Users/ilya/Desktop"
    };

    EventBuffer buffer(
            EventBuffer::Config(5, 3000),
            [](std::vector<FSEvent>&& batch){
                std::cout << "=== FLUSH batch size=" << batch.size() << " ===\n";
                for (const auto& e : batch) {
                    std::cout << "  id=" << e.identifier
                              << " path=" << e.path
                              << " flags=" << e.flags
                              << "\n";
                }
                std::cout << "===============================\n";
            }
    );

    FSEventsAgent agent(pathsToWatch, 0.5);
    bool started = agent.start([&](const std::vector<FSEvent>& events){
        buffer.addMany(events);
    });

    if (!started) {
        std::cerr << "Failed to start FSEventsAgent\n";
        return 1;
    }

    std::cout << "Watching... (Ctrl+C to stop)\n";
    CFRunLoopRun();

    buffer.flush();

    return 0;
}