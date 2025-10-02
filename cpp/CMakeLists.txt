cmake_minimum_required(VERSION 3.15)
project(FSEventsService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(FSEventsService agent/src/main.cpp
        agent/include/FSEventsAgent.h
        agent/src/FSEventsAgent.cpp)

target_link_libraries(FSEventsService "-framework CoreServices")