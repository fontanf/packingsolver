add_executable(PackingSolver_rectangleguillotine_test)
target_sources(PackingSolver_rectangleguillotine_test PRIVATE
    rectangleguillotine_test.cpp
    tree_search/waste_test.cpp
    tree_search/add_test.cpp
    tree_search/insertion_test.cpp
    tree_search/defect_test.cpp
    tree_search/cut_through_defect_test.cpp
    tree_search/trim_test.cpp
    tree_search/cut_thickness_test.cpp
    tree_search/integration_test.cpp
    tree_search/tree_search_test.cpp
    column_generation_strips_test.cpp
    sequential_strips_onedimensional_test.cpp
    dynamic_programming_infinite_copies_array_test.cpp
    labeling_test.cpp
    post_process_test.cpp)
target_include_directories(PackingSolver_rectangleguillotine_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_rectangleguillotine_test
    PackingSolver_rectangleguillotine
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver
    Boost::filesystem
    GTest::gtest_main)
gtest_discover_tests(PackingSolver_rectangleguillotine_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
