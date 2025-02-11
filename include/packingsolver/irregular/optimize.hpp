#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "columngenerationsolver/linear_programming_solver.hpp"

namespace packingsolver
{
namespace irregular
{

struct Output: packingsolver::Output<Instance, Solution>
{
    Output(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct OptimizeParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Linear programming solver. */
    columngenerationsolver::SolverName solver_name
        = columngenerationsolver::SolverName::CLP;

    /** Use tree search algorithm. */
    bool use_tree_search = false;

    /** Use sequential single knapsack algorithm. */
    bool use_sequential_single_knapsack = false;

    /** Use sequential value correction algorithm. */
    bool use_sequential_value_correction = false;

    /** Use dichotomic search algorithm. */
    bool use_dichotomic_search = false;

    /** Use column generation algorithm. */
    bool use_column_generation = false;

    /** Initial maximum approximation ratio. */
    double initial_maximum_approximation_ratio = 0.20;

    /** Decrease factor for the maximum approximation ratio. */
    double maximum_approximation_ratio_factor = 0.75;

    /** Guides used in the tree search algorithm. */
    std::vector<GuideId> tree_search_guides;

    /** Threshold to consider that a bin contains "many" items. */
    Counter many_items_in_bins_threshold = 16;

    /** Factor to consider that the number of copies of items is "high". */
    Counter many_item_type_copies_factor = 1;

    /**
     * Size of the queue for the pricing knapsack subproblem of the sequential
     * value correction algorithm.
     */
    NodeId sequential_value_correction_subproblem_queue_size = 128;

    /**
     * Size of the queue for the pricing knapsack subproblem of the column
     * generation algorithm.
     */
    NodeId column_generation_subproblem_queue_size = 128;

    /*
     * Parameters for non-anytime mode
     */

    /** Maximum approximation ratio. */
    double not_anytime_maximum_approximation_ratio = 0.05;

    /** Size of the queue in the tree search algorithm. */
    NodeId not_anytime_tree_search_queue_size = 512;

    /**
     * Size of the queue in the single knapsack subproblem of the sequential
     * single knapsack algorithm.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_queue_size = 512;

    /** Number of iterations of the sequential value correction algorithm. */
    Counter not_anytime_sequential_value_correction_number_of_iterations = 32;

    /**
     * Size of the queue in the bin packing subproblem of the dichotomic search
     * algorithm.
     */
    NodeId not_anytime_dichotomic_search_subproblem_queue_size = 128;
};

Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
