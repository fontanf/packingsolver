#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct OptimizeOptionalParameters
{
    /** Algorithm for Bin Packing problems. */
    Algorithm bpp_algorithm = Algorithm::Auto;

    /** Algorithm for Variable-sized Bin Packing problems. */
    Algorithm vbpp_algorithm = Algorithm::Auto;

    /** Size of the queue in the tree search algorithm. */
    NodeId tree_search_queue_size = -1;

    /**
     * Size of the queue for the pricing knapsack sub-problem of the column
     * generation algorithm.
     */
    NodeId column_generation_queue_size = 256;

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

