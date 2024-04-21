add_executable(PackingSolver_rectangleguillotine_test)
target_sources(PackingSolver_rectangleguillotine_test PRIVATE
    rectangleguillotine_test.cpp
    branching_scheme/waste_test.cpp
    branching_scheme/add_test.cpp
    branching_scheme/insertion_test.cpp
    branching_scheme/defect_test.cpp
    branching_scheme/cut_through_defect_test.cpp
    branching_scheme/trim_test.cpp
    branching_scheme/cut_thickness_test.cpp
    branching_scheme/integration_test.cpp)
target_link_libraries(PackingSolver_rectangleguillotine_test
    PackingSolver_rectangleguillotine
    PackingSolver_algorithms
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_rectangleguillotine_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
