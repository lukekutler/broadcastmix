cmake_minimum_required(VERSION 3.24)

project(BroadcastMix
        VERSION 3.0.0
        DESCRIPTION "BroadcastMix v3 live broadcast mixing application"
        LANGUAGES C CXX)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS version" FORCE)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BROADCASTMIX_BUILD_TESTS "Build unit tests" ON)
option(BROADCASTMIX_FETCH_JUCE "Fetch JUCE framework via FetchContent" ON)

if(BROADCASTMIX_FETCH_JUCE)
    include(cmake/Dependencies.cmake)
endif()

add_subdirectory(src)

if(BROADCASTMIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BROADCASTMIX_FETCH_JUCE)
    add_subdirectory(apps)
endif()
