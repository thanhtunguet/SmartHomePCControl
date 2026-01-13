cmake_minimum_required(VERSION 3.10)
project(SmartHomePCControl-CPP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(smarthome_pc_controller src/main.cpp)

# === RELEASE BUILD OPTIMIZATIONS === #
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Size optimization instead of speed (-Os instead of -O2/-O3)
    set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
    
    # Disable unused C++ features
    target_compile_options(smarthome_pc_controller PRIVATE
        -fno-exceptions          # No C++ exceptions
        -fno-rtti                # No runtime type info
        -fno-unwind-tables       # No unwind tables
        -fno-asynchronous-unwind-tables
        -ffunction-sections      # Put functions in separate sections
        -fdata-sections          # Put data in separate sections
    )
    
    # Linker optimizations
    if(APPLE)
        # macOS: Use dead_strip
        target_link_options(smarthome_pc_controller PRIVATE
            -Wl,-dead_strip
        )
        # Strip after build on macOS
        add_custom_command(TARGET smarthome_pc_controller POST_BUILD
            COMMAND strip $<TARGET_FILE:smarthome_pc_controller>
            COMMENT "Stripping binary..."
        )
    else()
        # Linux: Use gc-sections and strip
        target_link_options(smarthome_pc_controller PRIVATE
            -Wl,--gc-sections    # Remove unused sections
            -Wl,-s               # Strip symbols
        )
    endif()
endif()
