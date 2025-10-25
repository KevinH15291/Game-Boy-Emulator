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

add_executable(GBC
  src/GBC.cc
  src/CPU/SM83.cc
  src/MMU/bus.cc
  src/video/PPU.cc
  src/audio/APU.cc
  test.cc
)

if(ENABLE_EMSCRIPTEN)
  target_compile_options(GBC PUBLIC
    "-sUSE_SDL=3"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sASYNCIFY"
  )
  target_link_options(GBC PUBLIC
    "-sUSE_SDL=3"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sASYNCIFY"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    "-sEXPORTED_FUNCTIONS=['_main']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
  )
else()
  find_package(SDL3 CONFIG REQUIRED)
  target_link_libraries(GBC PRIVATE SDL3::SDL3)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

include_directories(src src/CPU src/MMU src/video src/audio)


