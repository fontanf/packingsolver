#pragma once

#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/solution.hpp"

#include "columngenerationsolver/linear_programming_solver.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct OptimizeOptionalParameters
{
    /** Number of threads. */
    Counter number_of_threads = 0;

    /** Algorithm for Bin Packing problems. */
    Algorithm bpp_algorithm = Algorithm::Auto;

    /** Algorithm for Variable-sized Bin Packing problems. */
    Algorithm vbpp_algorithm = Algorithm::Auto;


    /** Size of the queue in the tree search algorithm. */
    NodeId tree_search_queue_size = -1;

    /** Guides used in the tree search algorithm. */
    std::vector<GuideId> tree_search_guides;


    /**
     * Time limit for the vbpp2bpp bin packing sub-problem of the column
     * generation algorithm.
     */
    double column_generation_vbpp2bpp_time_limit = -1;

    /**
     * Size of the queue for the vbpp2bpp bin packing sub-problem of the column
     * generation algorithm.
     */
    NodeId column_generation_vbpp2bpp_queue_size = 1024;

    /**
     * Size of the queue for the pricing knapsack sub-problem of the column
     * generation algorithm.
     */
    NodeId column_generation_pricing_queue_size = 256;

    /** Linear programming solver. */
    columngenerationsolver::LinearProgrammingSolver linear_programming_solver
        = columngenerationsolver::LinearProgrammingSolver::CLP;


    /**
     * Size of the queue for the bin packing sub-problem of the dichotomic
     * search algorithm.
     */
    NodeId dichotomic_search_queue_size = 1024;


    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();
};

struct Output
{
    Output(const Instance& instance):
        solution_pool(instance, 1) { }

    SolutionPool<Instance, Solution> solution_pool;
};

Output optimize(
        const Instance& instance,
        OptimizeOptionalParameters parameters = {});

}
}

