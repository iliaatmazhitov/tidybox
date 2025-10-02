//
// Created by Ilya Atmazhitov on 29.09.2025.
//

#include <iostream>
#include "../include/FSEventsAgent.h"

using namespace std;


int main() {
    std::vector<std::string> pathsToWatch = {"/Users/ilya/Downloads", "/Users/ilya/Desktop"};

    FSEventsAgent agent(pathsToWatch, 2.0);

    agent.start([](const std::vector<FSEvent>& events){
        for (const auto& event : events) {
            std::cout << "Event ID: " << event.identifier << ", Path: " << event.path;

            if (event.flags & kFSEventStreamEventFlagItemCreated)
                std::cout << " [CREATED]";
            if (event.flags & kFSEventStreamEventFlagItemModified)
                std::cout << " [MODIFIED]";
            if (event.flags & kFSEventStreamEventFlagItemRemoved)
                std::cout << " [REMOVED]";
            if (event.flags & kFSEventStreamEventFlagItemRenamed)
                std::cout << " [RENAMED]";
            if (event.flags & kFSEventStreamEventFlagItemIsFile)
                std::cout << " [FILE]";
            if (event.flags & kFSEventStreamEventFlagItemIsDir)
                std::cout << " [DIR]";

            std::cout << std::endl;
        }
    });

    std::cout << "Starting CFRunLoop..." << std::endl;
    CFRunLoopRun();
    return 0;
}