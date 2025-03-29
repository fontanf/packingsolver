add_executable(PackingSolver_box_test)
target_sources(PackingSolver_box_test PRIVATE
    box_test.cpp
    tree_search_test.cpp)
target_include_directories(PackingSolver_box_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_box_test
    PackingSolver_box
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_box_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
