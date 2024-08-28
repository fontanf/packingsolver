find_package(Threads)
add_library(PackingSolver_irregular)
target_sources(PackingSolver_irregular PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    algorithm_formatter.cpp
    optimize.cpp
    shape.cpp
    polygon_convex_hull.cpp
    polygon_trapezoidation.cpp
    polygon_simplification.cpp
    branching_scheme.cpp)
target_include_directories(PackingSolver_irregular PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_include_directories(PackingSolver_irregular PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_irregular PUBLIC
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver
    Threads::Threads)
if (PackingSolver_USE_KNITRO)
    target_link_libraries(PackingSolver_irregular PUBLIC
        KnitroCpp::knitrocpp)
    target_compile_definitions(PackingSolver_irregular PRIVATE
        KNITRO_FOUND=1)
endif()
add_library(PackingSolver::irregular ALIAS PackingSolver_irregular)

add_executable(PackingSolver_irregular_main)
target_sources(PackingSolver_irregular_main PRIVATE
    main.cpp)
target_link_libraries(PackingSolver_irregular_main PUBLIC
    PackingSolver_irregular
    Boost::program_options)
set_target_properties(PackingSolver_irregular_main PROPERTIES OUTPUT_NAME "packingsolver_irregular")
install(TARGETS PackingSolver_irregular_main)
