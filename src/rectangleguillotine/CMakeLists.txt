find_package(Threads)
add_library(PackingSolver_rectangleguillotine)
target_sources(PackingSolver_rectangleguillotine PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    solution_builder.cpp
    algorithm_formatter.cpp
    optimize.cpp
    branching_scheme.cpp
    branching_scheme_n.cpp)
target_include_directories(PackingSolver_rectangleguillotine PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(PackingSolver_rectangleguillotine PUBLIC
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver
    Threads::Threads)
add_library(PackingSolver::rectangleguillotine ALIAS PackingSolver_rectangleguillotine)

add_executable(PackingSolver_rectangleguillotine_main)
target_sources(PackingSolver_rectangleguillotine_main PRIVATE
    main.cpp)
target_link_libraries(PackingSolver_rectangleguillotine_main PUBLIC
    PackingSolver_rectangleguillotine
    Boost::program_options)
set_target_properties(PackingSolver_rectangleguillotine_main PROPERTIES OUTPUT_NAME "packingsolver_rectangleguillotine")
install(TARGETS PackingSolver_rectangleguillotine_main)
