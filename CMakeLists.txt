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
option(ENABLE_PPU_DEBUG "Enable native PPU debug visualizers" ON)
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

include(FetchContent)

if(EMSCRIPTEN)
    set(SDL_SHARED OFF CACHE BOOL "Disable shared SDL builds" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Force static SDL" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Skip SDL tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Skip SDL examples" FORCE)
    set(SDL_NET OFF CACHE BOOL "Disable SDL networking" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "Disable SDL camera subsystem" FORCE)
    set(SDL_GPU OFF CACHE BOOL "Disable GPU subsystem" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "Disable HIDAPI subsystem" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "Disable haptic subsystem" FORCE)
    set(SDL_POWER OFF CACHE BOOL "Disable power subsystem" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "Disable sensor subsystem" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "Disable dialog subsystem" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "Disable tray subsystem" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "Disable joystick subsystem" FORCE)
    set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "Disable virtual joystick" FORCE)
    set(SDL_AUDIO ON CACHE BOOL "Enable SDL audio" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "Enable SDL video" FORCE)
    set(SDL_RENDER ON CACHE BOOL "Enable SDL render" FORCE)
    add_subdirectory(third_party/SDL3)
else()
    find_package(SDL3)
    if(NOT SDL3_FOUND)
        set(SDL_SHARED OFF CACHE BOOL "Disable shared SDL builds" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Force static SDL" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "Skip SDL tests" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "Skip SDL examples" FORCE)
        set(SDL_NET OFF CACHE BOOL "Disable SDL networking" FORCE)
        set(SDL_CAMERA OFF CACHE BOOL "Disable SDL camera subsystem" FORCE)
        set(SDL_GPU OFF CACHE BOOL "Disable GPU subsystem" FORCE)
        set(SDL_HIDAPI OFF CACHE BOOL "Disable HIDAPI subsystem" FORCE)
        set(SDL_HAPTIC OFF CACHE BOOL "Disable haptic subsystem" FORCE)
        set(SDL_POWER OFF CACHE BOOL "Disable power subsystem" FORCE)
        set(SDL_SENSOR OFF CACHE BOOL "Disable sensor subsystem" FORCE)
        set(SDL_DIALOG OFF CACHE BOOL "Disable dialog subsystem" FORCE)
        set(SDL_TRAY OFF CACHE BOOL "Disable tray subsystem" FORCE)
        set(SDL_JOYSTICK OFF CACHE BOOL "Disable joystick subsystem" FORCE)
        set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "Disable virtual joystick" FORCE)
        set(SDL_AUDIO ON CACHE BOOL "Enable SDL audio" FORCE)
        set(SDL_VIDEO ON CACHE BOOL "Enable SDL video" FORCE)
        set(SDL_RENDER ON CACHE BOOL "Enable SDL render" FORCE)
        add_subdirectory(third_party/SDL3)
    endif()
endif()

include_directories(src src/CPU src/MMU src/video src/audio)

# Source files
set(SOURCES
    main.cc
    src/GBC.cc
    src/CPU/SM83.cc
    src/MMU/bus.cc
    src/video/PPU.cc
    src/audio/APU.cc
)

# Create executable
add_executable(GBC ${SOURCES})

if(EMSCRIPTEN)
    target_compile_options(GBC PUBLIC 
        -O3 
        -flto 
        -fno-rtti 
        -fno-exceptions
        -ffast-math
        -fno-math-errno
        -fno-trapping-math
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables)
    target_include_directories(GBC PRIVATE ${SDL3_SOURCE_DIR}/include)
    target_link_options(GBC PUBLIC 
        -O3
        -flto
        -sSTACK_SIZE=2097152
        -sALLOW_MEMORY_GROWTH=1
        -sASSERTIONS=0
        -sDISABLE_EXCEPTION_CATCHING=1
        -sFILESYSTEM=1
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXIT_RUNTIME=1
        -sENVIRONMENT='web'
        --closure 1
        -sEXPORT_NAME=createGBCModule
        -sEXPORTED_FUNCTIONS=_main,_upload_rom,_set_button_state,_set_channel_muted,_is_channel_muted,_set_master_volume,_get_master_volume,_get_frame_count,_get_cart_ram_ptr,_get_cart_ram_size,_import_save,_shutdown_emulator,_malloc,_free
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,writeArrayToMemory,HEAPU8,HEAP8
        -sERROR_ON_UNDEFINED_SYMBOLS=1
        -sWARN_UNALIGNED=1)
    target_link_libraries(GBC PRIVATE SDL3::SDL3-static)

    # Post-build: Copy outputs to prebuilt-web (mirroring build_and_serve.sh)
    add_custom_command(TARGET GBC POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/prebuilt-web
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/shell.html
                ${CMAKE_SOURCE_DIR}/prebuilt-web/index.html
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/GBC.js
                ${CMAKE_SOURCE_DIR}/prebuilt-web/GBC.js
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/GBC.wasm
                ${CMAKE_SOURCE_DIR}/prebuilt-web/GBC.wasm
        COMMENT "Copying Emscripten outputs to prebuilt-web/"
    )
else()
    target_compile_options(GBC PRIVATE -O3 -march=native -mtune=native)

    if(SDL3_FOUND)
        target_link_libraries(GBC PRIVATE SDL3::SDL3)
    else()
        target_link_libraries(GBC PRIVATE SDL3::SDL3-static)
        target_include_directories(GBC PRIVATE ${SDL3_SOURCE_DIR}/include)
    endif()
    if(ENABLE_PPU_DEBUG)
        target_compile_definitions(GBC PRIVATE GBC_ENABLE_PPU_DEBUG)
    endif()
endif()

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8000000")
endif()

