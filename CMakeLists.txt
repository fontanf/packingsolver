cmake_minimum_required(VERSION 3.28)

project(PackingSolver LANGUAGES CXX)

# Solver options.
option(PACKINGSOLVER_USE_CLP "Use CLP" ON)
option(PACKINGSOLVER_USE_HIGHS "Use HiGHS" ON)
option(PACKINGSOLVER_USE_KNITRO "Use Knitro" OFF)

# Build options.
option(PACKINGSOLVER_BUILD_MAIN "Build main" ON)
option(PACKINGSOLVER_BUILD_TEST "Build unit tests" ON)
option(PACKINGSOLVER_BUILD_PYTHON "Build Python bindings" OFF)
option(PACKINGSOLVER_BUILD_WASM "Build the WebAssembly module (with Emscripten)" OFF)

# Avoid FetchContent warning.
cmake_policy(SET CMP0135 NEW)

# Require C++14.
set(CMAKE_CXX_STANDARD 14)

# The static libraries are linked into the Python extension module. The
# prebuilt CLP archives are not position independent, so CLP can't be linked
# into it: the Python module uses HiGHS instead.
if(PACKINGSOLVER_BUILD_PYTHON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if(PACKINGSOLVER_USE_CLP)
        message(STATUS "PACKINGSOLVER_BUILD_PYTHON: disabling CLP (prebuilt archives are not position independent)")
        set(PACKINGSOLVER_USE_CLP OFF)
    endif()
    if(NOT PACKINGSOLVER_USE_HIGHS)
        message(FATAL_ERROR "PACKINGSOLVER_BUILD_PYTHON requires PACKINGSOLVER_USE_HIGHS")
    endif()
endif()

# The WebAssembly module is built with Emscripten, with thread support (each
# thread runs in a Web Worker), so that every optimization mode works. CLP
# isn't available there: the module uses HiGHS. C++ exceptions are enabled
# (they are disabled by default with Emscripten), since the library reports
# invalid inputs with exceptions.
if(PACKINGSOLVER_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "PACKINGSOLVER_BUILD_WASM requires the Emscripten toolchain (use emcmake)")
    endif()
    if(PACKINGSOLVER_USE_CLP)
        message(STATUS "PACKINGSOLVER_BUILD_WASM: disabling CLP")
        set(PACKINGSOLVER_USE_CLP OFF)
    endif()
    if(NOT PACKINGSOLVER_USE_HIGHS)
        message(FATAL_ERROR "PACKINGSOLVER_BUILD_WASM requires PACKINGSOLVER_USE_HIGHS")
    endif()
    set(PACKINGSOLVER_BUILD_MAIN OFF)
    set(PACKINGSOLVER_BUILD_TEST OFF)
    add_compile_options(-fwasm-exceptions -pthread)
    add_link_options(-fwasm-exceptions -pthread)
endif()

# Enable output of compile commands during generation.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set MSVC_RUNTIME_LIBRARY.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Add sub-directories.
add_subdirectory(extern)
add_subdirectory(src)
if(PACKINGSOLVER_BUILD_TEST)
    add_subdirectory(test)
endif()
if(PACKINGSOLVER_BUILD_PYTHON)
    add_subdirectory(python)
endif()
if(PACKINGSOLVER_BUILD_WASM)
    add_subdirectory(wasm)
endif()
