add_executable(PackingSolver_algorithms_test)
target_sources(PackingSolver_algorithms_test PRIVATE
    meet_in_the_middle_test.cpp)
target_include_directories(PackingSolver_algorithms_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_algorithms_test
    PackingSolver_algorithms
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_algorithms_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
