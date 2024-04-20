add_executable(PackingSolver_boxstacks_test)
target_sources(PackingSolver_boxstacks_test PRIVATE
    boxstacks_test.cpp)
target_link_libraries(PackingSolver_boxstacks_test
    PackingSolver_boxstacks
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_boxstacks_test)
