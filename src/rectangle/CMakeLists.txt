find_package(Threads)
add_library(PackingSolver_rectangle)
target_sources(PackingSolver_rectangle PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    algorithm_formatter.cpp
    optimize.cpp
    branching_scheme.cpp)
target_include_directories(PackingSolver_rectangle PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(PackingSolver_rectangle PUBLIC
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver
    Threads::Threads)
add_library(PackingSolver::rectangle ALIAS PackingSolver_rectangle)

add_executable(PackingSolver_rectangle_main)
target_sources(PackingSolver_rectangle_main PRIVATE
    main.cpp)
target_link_libraries(PackingSolver_rectangle_main PUBLIC
    PackingSolver_rectangle
    Boost::program_options)
set_target_properties(PackingSolver_rectangle_main PROPERTIES OUTPUT_NAME "packingsolver_rectangle")
install(TARGETS PackingSolver_rectangle_main)
