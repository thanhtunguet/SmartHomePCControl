cmake_minimum_required(VERSION 3.10)
project(LinuxPCControl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Executables
add_executable(shutdown-listener src/shutdown-listener.cpp)
target_link_libraries(shutdown-listener PRIVATE Threads::Threads)

add_executable(wakeup src/wakeup.cpp)

# Installation
include(GNUInstallDirs)
install(TARGETS shutdown-listener wakeup
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
