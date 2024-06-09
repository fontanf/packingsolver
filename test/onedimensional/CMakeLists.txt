add_executable(PackingSolver_onedimensional_test)
target_sources(PackingSolver_onedimensional_test PRIVATE
    onedimensional_test.cpp)
target_include_directories(PackingSolver_onedimensional_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_onedimensional_test
    PackingSolver_onedimensional
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_onedimensional_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
