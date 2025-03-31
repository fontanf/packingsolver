add_executable(PackingSolver_irregular_test)
target_sources(PackingSolver_irregular_test PRIVATE
    trapezoid_test.cpp
    shape_test.cpp
    shape_element_intersections_test.cpp
    shape_convex_hull_test.cpp
    shape_extract_borders_test.cpp
    shape_self_intersections_removal_test.cpp
    shape_trapezoidation_test.cpp
    irregular_test.cpp)
target_include_directories(PackingSolver_irregular_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
