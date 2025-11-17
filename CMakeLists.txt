cmake_minimum_required(VERSION 3.16)
project(GBC)


if(EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".js")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EMSCRIPTEN)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Profiling options
option(ENABLE_PROFILING "Enable profiling instrumentation" OFF)
if(ENABLE_PROFILING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -O2")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg -O2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    message(STATUS "Profiling enabled with gprof")
endif()


option(ENABLE_LTO "Enable Link-Time Optimization" ON)
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT output)
    if(LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "Link-Time Optimization (LTO) enabled")
    else()
        message(STATUS "LTO not supported: ${output}")
    endif()
endif()

if(EMSCRIPTEN)
    # SDL2 port is used for Emscripten (stable, well-tested)
    # native SDL3 is bugged
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

if(EMSCRIPTEN)
    target_compile_options(GBC PUBLIC -sUSE_SDL=2 -O3)
    target_link_options(GBC PUBLIC 
        -flto
        -sUSE_SDL=2
        -sSTACK_SIZE=16777216
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createGBCModule
        -sEXPORTED_FUNCTIONS=_main,_upload_rom,_set_button_state,_set_channel_muted,_is_channel_muted,_set_master_volume,_get_master_volume,_get_frame_count,_get_cart_ram_ptr,_get_cart_ram_size,_import_save,_malloc,_free
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,writeArrayToMemory,HEAPU8,HEAP8
    )
else()
    target_link_libraries(GBC PRIVATE SDL3::SDL3)
endif()

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8000000")
endif()
