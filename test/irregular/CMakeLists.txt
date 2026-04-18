add_executable(PackingSolver_irregular_test)
target_sources(PackingSolver_irregular_test PRIVATE
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

add_executable(PackingSolver_irregular_linear_programming_test)
target_sources(PackingSolver_irregular_linear_programming_test PRIVATE
    linear_programming_test.cpp)
target_include_directories(PackingSolver_irregular_linear_programming_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular_linear_programming_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_linear_programming_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
