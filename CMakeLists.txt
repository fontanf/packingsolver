cmake_minimum_required(VERSION 3.15.0)

project(PackingSolver LANGUAGES CXX)

option(PackingSolver_USE_KNITRO "Use Knitro" OFF)

# Require C++14.
set(CMAKE_CXX_STANDARD 14)

# Enable output of compile commands during generation.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set MSVC_RUNTIME_LIBRARY.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# https://stackoverflow.com/questions/71056191/ld-library-path-for-gtest-discover-tests-in-qtcreator
# https://stackoverflow.com/questions/63402829/how-to-add-linker-directories-to-cmake-gtest-discover-tests/66394189#66394189
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Add sub-directories.
add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(test)
