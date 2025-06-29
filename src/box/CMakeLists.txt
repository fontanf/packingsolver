find_package(Threads)
add_library(PackingSolver_box)
target_sources(PackingSolver_box PRIVATE
    instance.cpp
    instance_builder.cpp
    solution.cpp
    instance_flipper.cpp
    algorithm_formatter.cpp
    optimize.cpp
    branching_scheme.cpp)
target_include_directories(PackingSolver_box PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_include_directories(PackingSolver_box PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PackingSolver_box PUBLIC
    PackingSolver_algorithms
    PackingSolver_rectangle
    PackingSolver_onedimensional
    KnapsackSolver::multiple_choice_subset_sum::dynamic_programming_bellman
    Threads::Threads)
add_library(PackingSolver::box ALIAS PackingSolver_box)

if(PACKINGSOLVER_BUILD_MAIN)
  add_executable(PackingSolver_box_main)
  target_sources(PackingSolver_box_main PRIVATE
      main.cpp)
  target_link_libraries(PackingSolver_box_main PUBLIC
      PackingSolver_box
      Boost::program_options
      Boost::filesystem)
  set_target_properties(PackingSolver_box_main PROPERTIES OUTPUT_NAME "packingsolver_box")
  install(TARGETS PackingSolver_box_main)
endif()
