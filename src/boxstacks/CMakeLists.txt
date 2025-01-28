find_package(Threads)
add_library(PackingSolver_boxstacks)
target_sources(PackingSolver_boxstacks PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    instance_flipper.cpp
    algorithm_formatter.cpp
    optimize.cpp
    sequential_onedimensional_rectangle.cpp
    branching_scheme.cpp)
target_include_directories(PackingSolver_boxstacks PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_include_directories(PackingSolver_boxstacks PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_boxstacks PUBLIC
    PackingSolver_algorithms
    PackingSolver_rectangle
    PackingSolver_onedimensional
    KnapsackSolver::multiple_choice_subset_sum::dynamic_programming_bellman
    Threads::Threads)
add_library(PackingSolver::boxstacks ALIAS PackingSolver_boxstacks)

add_executable(PackingSolver_boxstacks_main)
target_sources(PackingSolver_boxstacks_main PRIVATE
    main.cpp)
target_link_libraries(PackingSolver_boxstacks_main PUBLIC
    PackingSolver_boxstacks
    Boost::program_options
    Boost::filesystem)
set_target_properties(PackingSolver_boxstacks_main PROPERTIES OUTPUT_NAME "packingsolver_boxstacks")
install(TARGETS PackingSolver_boxstacks_main)
