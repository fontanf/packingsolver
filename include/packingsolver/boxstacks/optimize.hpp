#pragma once

#include "packingsolver/boxstacks/solution.hpp"

#include "packingsolver/boxstacks/sequential_onedimensional_rectangle.hpp"
#include "packingsolver/algorithms/sequential_value_correction.hpp"

#include "columngenerationsolver/linear_programming_solver.hpp"

namespace packingsolver
{
namespace boxstacks
{

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

using NewSolutionCallback = std::function<void(const Output&)>;

struct OptimizeParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** New solution callback. */
    NewSolutionCallback new_solution_callback = [](const Output&) { };

    /** Linear programming solver. */
    columngenerationsolver::LinearProgrammingSolver linear_programming_solver
        = columngenerationsolver::LinearProgrammingSolver::CLP;

    /** Use tree search algorithm. */
    bool use_tree_search = false;

    /** Use sequential single knapsack algorithm. */
    bool use_sequential_single_knapsack = false;

    /** Parameters of the sequential_onedimensional_rectangle algorithm. */
    SequentialOneDimensionalRectangleParameters sequential_onedimensional_rectangle_parameters;

    /** Guides used in the tree search algorithm. */
    std::vector<GuideId> tree_search_guides;

    /**
     * Size of the queue for the pricing knapsack subproblem of the sequential
     * value correction algorithm.
     */
    NodeId sequential_value_correction_subproblem_queue_size = 1024;

    /**
     * Size of the queue for the pricing knapsack subproblem of the column
     * generation algorithm.
     */
    NodeId column_generation_subproblem_queue_size = 1024;

    /*
     * Parameters for non-anytime mode
     */

    /** Size of the queue in the tree search algorithm. */
    NodeId not_anytime_tree_search_queue_size = 1024;

    /**
     * Size of the queue in the single knapsack subproblem of the sequential
     * single knapsack algorithm.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_queue_size = 1024;

    /** Number of iterations of the sequential value correction algorithm. */
    Counter not_anytime_sequential_value_correction_number_of_iterations = 32;
};

const Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
