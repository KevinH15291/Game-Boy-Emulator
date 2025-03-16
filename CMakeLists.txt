cmake_minimum_required(VERSION 3.22)
project(TestApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(GBC test.cc src/GBC.cc src/CPU/SM83.cc src/video/PPU.cc src/MMU/bus.cc src/audio/APU.cc) 

target_include_directories(GBC PUBLIC src)
target_include_directories(GBC PUBLIC src/CPU)
target_include_directories(GBC PUBLIC src/MMU)
target_include_directories(GBC PUBLIC src/video)
target_include_directories(GBC PUBLIC src/audio)



target_compile_options(GBC PUBLIC -fwrapv)

find_package(SDL3)

target_link_libraries(GBC SDL3::SDL3)

add_subdirectory(src/audio)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8000000")
endif()
