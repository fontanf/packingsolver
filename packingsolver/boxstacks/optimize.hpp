#pragma once

#include "packingsolver/boxstacks/solution.hpp"

#include "packingsolver/boxstacks/sequential_onedimensional_rectangle.hpp"
#include "packingsolver/algorithms/sequential_value_correction.hpp"

#include "columngenerationsolver/linear_programming_solver.hpp"

namespace packingsolver
{
namespace boxstacks
{

struct OptimizeParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;


    /** Parameters of the sequential_onedimensional_rectangle algorithm. */
    SequentialOneDimensionalRectangleParameters sequential_onedimensional_rectangle_parameters;

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
     * Size of the queue for the VbppToBpp bin packing sub-problem of the
     * column generation algorithm.
     */
    NodeId column_generation_vbpp_to_bpp_queue_size = 256;

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
    NodeId dichotomic_search_queue_size = 32;


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


    /**
     * Number of items in the solution found by the Sequential onedimensional
     * rectangle algorithm, before the repair step.
     */
    ItemPos sequential_onedimensional_rectangle_number_of_items = -1;

    /**
     * Profit of the solution found by the Sequential onedimensional rectangle
     * algorithm, after the repair step.
     */
    Profit sequential_onedimensional_rectangle_profit = -1;

    /** Time spent in the Sequential onedimensional rectangle algorithm. */
    double sequential_onedimensional_rectangle_time = 0.0;

    /**
     * Time spent in the onedimensional subproblem of the Sequential
     * onedimensional rectangle algorithm.
     */
    double sequential_onedimensional_rectangle_onedimensional_time = 0.0;

    /**
     * Time spent in the rectangle subproblem of the Sequential onedimensional
     * rectangle algorithm.
     */
    double sequential_onedimensional_rectangle_rectangle_time = 0.0;

    /** Time spent in the 'boxstacks' branching scheme. */
    double tree_search_time = 0.0;

    /**
     * Number of calls to the Sequential onedimensional rectangle algorithm.
     */
    Counter number_of_sequential_onedimensional_rectangle_calls = 0;

    /**
     * Number of times the Sequential onedimensional rectangle algorithm
     * managed to pack all items.
     */
    Counter number_of_sequential_onedimensional_rectangle_perfect = 0;

    /**
     * Number of times the Sequential onedimensional rectangle algorithm did
     * not generate a solution violating the axle weight constraints before the
     * repair step.
     */
    Counter number_of_sequential_onedimensional_rectangle_good = 0;

    /** Number of calls to the Tree Search algorithm. */
    Counter number_of_tree_search_calls = 0;

    /** Number of times the Tree Search algorithm managed to pack all items. */
    Counter number_of_tree_search_perfect = 0;

    /**
     * Number of times the Tree Search algorithm found a better solution than
     * the Sequential onedimensional rectangle algorithm.
     */
    Counter number_of_tree_search_better = 0;
};

const Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
