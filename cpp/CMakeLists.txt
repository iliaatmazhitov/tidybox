cmake_minimum_required(VERSION 3.15)
project(tidybox-agent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(agent
        agent/src/main.cpp
        agent/src/FSEventsAgent.cpp
        agent/include/EventBuffer.h
        agent/src/EventBuffer.cpp
)

target_include_directories(agent PRIVATE
        agent/include
        agent/src
)

find_package(CURL REQUIRED)
target_link_libraries(agent CURL::libcurl)

if(APPLE)
    target_link_libraries(agent "-framework CoreServices")
endif()

install(TARGETS agent DESTINATION bin)