find_package(Threads)
add_library(PackingSolver_rectangleguillotine)
target_sources(PackingSolver_rectangleguillotine PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    solution_builder.cpp
    instance_flipper.cpp
    algorithm_formatter.cpp
    optimize.cpp
    branching_scheme.cpp
    column_generation_2.cpp)
target_include_directories(PackingSolver_rectangleguillotine PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_include_directories(PackingSolver_rectangleguillotine PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_rectangleguillotine PUBLIC
    PackingSolver_algorithms
    PackingSolver_onedimensional
    PackingSolver_rectangle
    TreeSearchSolver::treesearchsolver
    Threads::Threads)
add_library(PackingSolver::rectangleguillotine ALIAS PackingSolver_rectangleguillotine)

if(PACKINGSOLVER_BUILD_MAIN)
  add_executable(PackingSolver_rectangleguillotine_main)
  target_sources(PackingSolver_rectangleguillotine_main PRIVATE
      main.cpp)
  target_link_libraries(PackingSolver_rectangleguillotine_main PUBLIC
      PackingSolver_rectangleguillotine
      Boost::program_options
      Boost::filesystem)
  set_target_properties(PackingSolver_rectangleguillotine_main PROPERTIES OUTPUT_NAME "packingsolver_rectangleguillotine")
  install(TARGETS PackingSolver_rectangleguillotine_main)
endif()
