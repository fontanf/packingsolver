add_executable(PackingSolver_irregular_test)
target_sources(PackingSolver_irregular_test PRIVATE
    irregular_test.cpp)
target_link_libraries(PackingSolver_irregular_test
    PackingSolver_irregular
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_irregular_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
