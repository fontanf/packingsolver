add_executable(PackingSolver_irregular_test)
target_sources(PackingSolver_irregular_test PRIVATE
    irregular_test.cpp
    milp_raster_test.cpp
    milp_nfp_test.cpp
    benders_decomposition_test.cpp
    local_search_test.cpp
    reduction_test.cpp
    sequential_feasibility_test.cpp
    tree_search_test.cpp
    trivial_test.cpp
    utils_test.cpp
    black_box_optimization/no_fit_polygon_test.cpp
    direct_trigonometry/polygon_test.cpp
    nonlinear_programming/separating_line_test.cpp)
target_include_directories(PackingSolver_irregular_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PackingSolver_irregular_rotations_test)
target_sources(PackingSolver_irregular_rotations_test PRIVATE
    rotations_test.cpp)
target_include_directories(PackingSolver_irregular_rotations_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_rotations_test
    PackingSolver_irregular
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_rotations_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PackingSolver_irregular_periodic_packing_test)
target_sources(PackingSolver_irregular_periodic_packing_test PRIVATE
    periodic_packing_test.cpp)
target_include_directories(PackingSolver_irregular_periodic_packing_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_periodic_packing_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_periodic_packing_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PackingSolver_irregular_tree_search_periodic_packing_test)
target_sources(PackingSolver_irregular_tree_search_periodic_packing_test PRIVATE
    tree_search_periodic_packing_test.cpp)
target_include_directories(PackingSolver_irregular_tree_search_periodic_packing_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_tree_search_periodic_packing_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_tree_search_periodic_packing_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PackingSolver_irregular_tree_search_maximal_spaces_test)
target_sources(PackingSolver_irregular_tree_search_maximal_spaces_test PRIVATE
    tree_search_maximal_spaces_test.cpp)
target_include_directories(PackingSolver_irregular_tree_search_maximal_spaces_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_tree_search_maximal_spaces_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_tree_search_maximal_spaces_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PackingSolver_irregular_linear_programming_test)
target_sources(PackingSolver_irregular_linear_programming_test PRIVATE
    linear_programming/anchor_test.cpp)
target_include_directories(PackingSolver_irregular_linear_programming_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_linear_programming_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_linear_programming_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
