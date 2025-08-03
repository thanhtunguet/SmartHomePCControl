cmake_minimum_required(VERSION 3.10)
project(SmartHomePCControl-CPP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(smarthome_pc_controller src/main.cpp)

# Add the vendor directory to the include path
target_include_directories(smarthome_pc_controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)


