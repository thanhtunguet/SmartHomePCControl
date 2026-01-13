cmake_minimum_required(VERSION 3.10)
project(SmartHomePCControl-CPP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(smarthome_pc_controller src/main.cpp)

# Optimization flags for release builds
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Strip symbols for minimal binary size (Linux only)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
endif()


