add_executable(PackingSolver_rectangle_test)
target_sources(PackingSolver_rectangle_test PRIVATE
    rectangle_test.cpp
    benders_decomposition_test.cpp
    branching_scheme/test.cpp
    branching_scheme/defects_test.cpp
    branching_scheme/unloading_test.cpp
    branching_scheme/integration_test.cpp)
target_include_directories(PackingSolver_rectangle_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_rectangle_test
    PackingSolver_rectangle
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_rectangle_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
