cmake_minimum_required(VERSION 3.16)
project(GBC)


if(EMSCRIPTEN)
  set(gbc_executable_suffix ".js")
endif()

if(NOT EMSCRIPTEN)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Profiling options
option(ENABLE_PROFILING "Enable profiling instrumentation" OFF)
if(ENABLE_PROFILING)
    if(EMSCRIPTEN)
        message(STATUS "Emscripten profiling enabled")
    else()
        message(STATUS "Native profiling enabled with gprof")
    endif()
endif()


option(ENABLE_LTO "Enable Link-Time Optimization" ON)
option(ENABLE_PPU_DEBUG "Enable native PPU debug visualizers" OFF)
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT output)
    if(NOT LTO_SUPPORTED)
        message(STATUS "LTO not supported: ${output}")
    endif()
endif()

include(FetchContent)

if(EMSCRIPTEN)
    add_compile_options(
        -O3
        -flto 
        -fno-rtti 
        -fno-exceptions
        -ffast-math
        -fno-math-errno
        -fno-trapping-math
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
        -fomit-frame-pointer
        -fvisibility=hidden
    )
    
    set(SDL_SHARED OFF CACHE BOOL "Disable shared SDL builds" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Force static SDL" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Disable SDL test library" FORCE)
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
    set(SDL_OFFSCREEN OFF CACHE BOOL "Disable offscreen video driver" FORCE)
    set(SDL_DUMMYVIDEO OFF CACHE BOOL "Disable dummy video driver" FORCE)
    set(SDL_DUMMYAUDIO OFF CACHE BOOL "Disable dummy audio driver" FORCE)
    set(SDL_DISKAUDIO OFF CACHE BOOL "Disable disk audio driver" FORCE)
    set(SDL_AUDIO ON CACHE BOOL "Enable SDL audio" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "Enable SDL video" FORCE)
    set(SDL_RENDER ON CACHE BOOL "Enable SDL render" FORCE)
    add_subdirectory(third_party/SDL3)
    target_compile_definitions(SDL3-static PRIVATE
        SDL_LEAN_AND_MEAN=1
        SDL_DISABLE_INVALID_PARAMS=1)
else()
    find_package(SDL3)
    if(NOT SDL3_FOUND)
        set(SDL_SHARED OFF CACHE BOOL "Disable shared SDL builds" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Force static SDL" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Disable SDL test library" FORCE)
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

target_compile_features(GBC PRIVATE cxx_std_23)
target_include_directories(GBC PRIVATE src src/CPU src/MMU src/video src/audio)

if(EMSCRIPTEN)
    set_target_properties(GBC PROPERTIES SUFFIX "${gbc_executable_suffix}")
endif()

if(ENABLE_PROFILING)
    if(EMSCRIPTEN)
        target_compile_options(GBC PRIVATE -g)
        target_link_options(GBC PRIVATE -sPROFILING_FUNCS=1 -sDEMANGLE_SUPPORT=1)
    else()
        target_compile_options(GBC PRIVATE -pg -g -O3)
        target_link_options(GBC PRIVATE -pg)
    endif()
endif()

if(ENABLE_LTO AND LTO_SUPPORTED)
    set_property(TARGET GBC PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "Link-Time Optimization (LTO) enabled")
endif()

if(EMSCRIPTEN)
    target_compile_options(GBC PRIVATE 
        -O3
        -flto 
        -fno-rtti 
        -fno-exceptions
        -ffast-math
        -fno-math-errno
        -fno-trapping-math
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
        -fomit-frame-pointer
        -fvisibility=hidden)
    target_include_directories(GBC PRIVATE ${SDL3_SOURCE_DIR}/include)
    target_link_options(GBC PRIVATE 
        --closed-world
        --ignore-implicit-traps
        --traps-never-happen
        --enable-bulk-memory-opt
        --enable-nontrapping-float-to-int
        --remove-unused-module-elements
        --remove-unused-nonfunction-module-elements
        --strip-debug
        --strip-producers
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
        -sWARN_UNALIGNED=1
        -sEXPORT_ALL=0
        -sBINARYEN_EXTRA_PASSES="-O3,--gufa,-O4,--converge")
    target_link_libraries(GBC PRIVATE SDL3::SDL3-static)

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
    target_compile_options(GBC PRIVATE -O3 -march=native -mtune=native -Wall)

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

if(MSVC)
    target_link_options(GBC PRIVATE /STACK:8000000)
endif()
