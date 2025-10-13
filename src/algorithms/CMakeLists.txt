add_library(PackingSolver_common)
target_sources(PackingSolver_common PRIVATE
    common.cpp)
target_include_directories(PackingSolver_common PUBLIC
    ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(PackingSolver_common PUBLIC
    OptimizationTools_utils)
add_library(PackingSolver::common ALIAS PackingSolver_common)

add_library(PackingSolver_algorithms INTERFACE)
target_include_directories(PackingSolver_algorithms INTERFACE
    ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(PackingSolver_algorithms INTERFACE
    PackingSolver_common
    ColumnGenerationSolver::columngenerationsolver
    KnapsackSolver::dynamic_programming_primal_dual)
add_library(PackingSolver::algorithms ALIAS PackingSolver_algorithms)
