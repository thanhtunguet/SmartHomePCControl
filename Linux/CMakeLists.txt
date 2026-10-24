cmake_minimum_required(VERSION 3.10)
project(LinuxPCControl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(CURL)

# Executables
add_executable(shutdown-listener src/shutdown-listener.cpp)
target_link_libraries(shutdown-listener PRIVATE Threads::Threads)

add_executable(wakeup src/wakeup.cpp)

set(INSTALL_TARGETS shutdown-listener wakeup)

if(CURL_FOUND)
    add_executable(dynamic-dns src/dynamic-dns.cpp)
    target_link_libraries(dynamic-dns PRIVATE CURL::libcurl)
    list(APPEND INSTALL_TARGETS dynamic-dns)
else()
    message(WARNING "CURL development package not found. Skipping dynamic-dns target. "
                    "To build dynamic-dns, install libcurl4-openssl-dev (Debian/Ubuntu/Raspberry Pi OS).")
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS ${INSTALL_TARGETS}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
