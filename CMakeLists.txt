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

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8000000")
endif()
