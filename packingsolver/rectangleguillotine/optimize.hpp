#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"
#include "packingsolver/algorithms/sequential_value_correction.hpp"

#include "columngenerationsolver/linear_programming_solver.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct OptimizeParameters: packingsolver::Parameters<Instance, Solution>
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
     * Time limit for the VbppToBpp bin packing sub-problem of the column
     * generation algorithm.
     */
    double column_generation_vbpp_to_bpp_time_limit = -1;

    /**
     * Size of the queue for the VbppToBpp bin packing sub-problem of the column
     * generation algorithm.
     */
    NodeId column_generation_vbpp_to_bpp_queue_size = 1024;

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


    /** Parameters for the Sequential Value Correction algorithm. */
    SequentialValueCorrectionParameters<Instance, Solution> sequential_value_correction_parameters;

    /**
     * Size of the queue for the knapsack sub-problem of the sequential value
     * correction algorithm.
     */
    NodeId sequential_value_correction_queue_size = 1024;
};

struct Output: packingsolver::Output<Instance, Solution>
{
    Output(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

const Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
