cmake_minimum_required(VERSION 3.16)
project(GBC)

option(ENABLE_EMSCRIPTEN "Build with Emscripten for WebAssembly" OFF)

if(ENABLE_EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Profiling options
option(ENABLE_PROFILING "Enable profiling instrumentation" OFF)
if(ENABLE_PROFILING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -O2")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg -O2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    message(STATUS "Profiling enabled with gprof")
endif()

if(EMSCRIPTEN)
    # Use SDL2 port for Emscripten (stable, well-tested)
    # Don't find native SDL3 - use Emscripten SDL2 port only
    # The port provides headers via -sUSE_SDL=2
else()
    find_package(SDL3 REQUIRED)
endif()

include_directories(src src/CPU src/MMU src/video src/audio)

# Source files
set(SOURCES
    test.cc
    src/GBC.cc
    src/CPU/SM83.cc
    src/MMU/bus.cc
    src/video/PPU.cc
    src/audio/APU.cc
)

# Create executable
add_executable(GBC ${SOURCES})

# Link libraries
if(EMSCRIPTEN)
    # SDL3 is provided via Emscripten port (-sUSE_SDL=3)
    # No need to link against native SDL3
else()
    target_link_libraries(GBC PRIVATE SDL3::SDL3)
endif()

# Compiler options
if(EMSCRIPTEN)
    target_compile_options(GBC PUBLIC -O3 -flto)
    target_link_options(GBC PUBLIC 
        -O3 -flto 
        -sASSERTIONS=0 
        -sDISABLE_EXCEPTION_CATCHING=1 
        -sMALLOC=emmalloc
        -sUSE_SDL=2
        -sSTACK_SIZE=16777216
        -sEXPORTED_FUNCTIONS=_main,_upload_rom,_set_button_state,_set_channel_muted,_is_channel_muted,_get_frame_count,_malloc,_free
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,writeArrayToMemory
    )
else()
    target_compile_options(GBC PUBLIC -O3)
    # Increase stack size to 16MB (default is usually 8MB on macOS)
    if(APPLE)
        # macOS uses -Wl,-stack_size,<size> where size is in bytes (hex)
        target_link_options(GBC PUBLIC -Wl,-stack_size,0x1000000)
    elseif(UNIX)
        target_link_options(GBC PUBLIC -Wl,-z,stack-size=16777216)
    elseif(WIN32)
        target_link_options(GBC PUBLIC /STACK:16777216)
    endif()
endif()
