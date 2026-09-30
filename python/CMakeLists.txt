find_package(Python 3.8
    COMPONENTS Interpreter Development.Module
    OPTIONAL_COMPONENTS Development.SABIModule
    REQUIRED)

# Fetch nanobind.
FetchContent_Declare(
    nanobind
    GIT_REPOSITORY https://github.com/wjakob/nanobind.git
    GIT_TAG v3.1.0
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(nanobind)

# nanobind requires C++17; only the binding sources are compiled with it, the
# libraries themselves stay C++14.
nanobind_add_module(_packingsolver
    NB_STATIC
    STABLE_ABI
    src/module.cpp
    src/common.cpp
    src/rectangleguillotine.cpp
    src/rectangle.cpp
    src/box.cpp
    src/boxstacks.cpp
    src/onedimensional.cpp
    src/irregular.cpp)
target_compile_features(_packingsolver PRIVATE cxx_std_17)
if(PACKINGSOLVER_USE_CLP)
    target_compile_definitions(_packingsolver PRIVATE PACKINGSOLVER_PYTHON_USE_CLP=1)
else()
    target_compile_definitions(_packingsolver PRIVATE PACKINGSOLVER_PYTHON_USE_CLP=0)
endif()
target_link_libraries(_packingsolver PRIVATE
    PackingSolver_rectangleguillotine
    PackingSolver_rectangle
    PackingSolver_box
    PackingSolver_boxstacks
    PackingSolver_onedimensional
    PackingSolver_irregular)

# In the build tree, lay out an importable 'packingsolver' package next to the
# extension module, so that the tests can run with
# 'PYTHONPATH=<build>/python'.
set_target_properties(_packingsolver PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packingsolver")
add_custom_command(TARGET _packingsolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/packingsolver"
        "${CMAKE_CURRENT_BINARY_DIR}/packingsolver")

install(TARGETS _packingsolver LIBRARY DESTINATION packingsolver)

if(PACKINGSOLVER_BUILD_TEST)
    enable_testing()
    add_test(
        NAME PackingSolver_python_test
        COMMAND ${Python_EXECUTABLE} -m pytest "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(PackingSolver_python_test PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}")
endif()
