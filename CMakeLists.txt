cmake_minimum_required(VERSION 3.15.0)

project(PackingSolver LANGUAGES CXX)

option(PackingSolver_BUILD_TEST "Build the unit tests" ON)
option(PackingSolver_USE_KNITRO "Use Knitro" OFF)

# Require C++14.
set(CMAKE_CXX_STANDARD 14)

# Enable output of compile commands during generation.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set MSVC_RUNTIME_LIBRARY.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Add sub-directories.
add_subdirectory(extern)
add_subdirectory(src)
if(PackingSolver_BUILD_TEST)
    add_subdirectory(test)
endif()
