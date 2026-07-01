add_executable(PackingSolver_rectangle_test)
target_sources(PackingSolver_rectangle_test PRIVATE
    rectangle_test.cpp
    benders_decomposition_test.cpp
    tree_search/test.cpp
    tree_search/defects_test.cpp
    tree_search/unloading_test.cpp
    tree_search/integration_test.cpp)
target_include_directories(PackingSolver_rectangle_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_rectangle_test
    PackingSolver_rectangle
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_rectangle_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
