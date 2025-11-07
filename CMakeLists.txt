cmake_minimum_required(VERSION 3.16)
project(CGB)

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
    target_compile_options(GBC PUBLIC -O3 -flto)
    target_link_options(GBC PUBLIC -O3 -flto -sASSERTIONS=0 -sDISABLE_EXCEPTION_CATCHING=1 -sMALLOC=emmalloc)
else()
    target_compile_options(GBC PUBLIC -O3)
endif()




if(EMSCRIPTEN)
    target_compile_options(GBC PUBLIC -O3 -flto)
    target_link_options(GBC PUBLIC -O3 -flto -sASSERTIONS=0 -sDISABLE_EXCEPTION_CATCHING=1 -sMALLOC=emmalloc)
else()
    target_compile_options(GBC PUBLIC -O3)
endif()

find_package(SDL3)

include_directories(src src/CPU src/MMU src/video src/audio)


