add_library(PackingSolver_irregular)
target_sources(PackingSolver_irregular PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    algorithm_formatter.cpp
    optimize.cpp
    nlp_circle.cpp
    nlp.cpp)
target_include_directories(PackingSolver_irregular PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(PackingSolver_irregular PUBLIC
    PackingSolver_algorithms
    TreeSearchSolver::treesearchsolver)
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