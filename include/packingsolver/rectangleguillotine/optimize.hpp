#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace rectangleguillotine
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

    /**
     * Memory limit in mebibytes.
     *
     * 0 (the default) means "unlimited": the optimization is never stopped
     * because of memory usage.
     */
    Megabytes memory_limit_megabytes = 0;

    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /** Use tree search algorithm. */
    bool use_tree_search = false;

    /** Use column generation strips algorithm. */
    bool use_column_generation_strips = false;

    /** Use sequential strips onedimensional algorithm. */
    bool use_sequential_strips_onedimensional = false;

    /** Use dynamic programming (infinite copies, array) algorithm. */
    bool use_dynamic_programming_infinite_copies_array = false;

    /** Use tree search maximal spaces algorithm. */
    bool use_tree_search_maximal_spaces = false;

    /** Use labeling algorithm. */
    bool use_labeling = false;

    /** Use sequential single knapsack algorithm. */
    bool use_sequential_single_knapsack = false;

    /** Use sequential value correction algorithm. */
    bool use_sequential_value_correction = false;

    /** Use dichotomic search algorithm. */
    bool use_dichotomic_search = false;

    /** Use column generation algorithm. */
    bool use_column_generation = false;

    /** Guides used in the tree search algorithm. */
    std::vector<GuideId> tree_search_guides;

    /** Threshold to consider that a bin contains "many" items. */
    Counter many_items_in_bins_threshold = 16;

    /** Threshold to consider that a bin contains "many" items. */
    Counter many_items_in_bins_threshold_2 = 64;

    /** Factor to consider that the number of copies of items is "high". */
    Counter many_item_type_copies_factor = 1;

    /**
     * Size of the queue in the tree search algorithm for the pricing knapsack subproblem of
     * the sequential value correction algorithm.
     */
    NodeId sequential_value_correction_subproblem_tree_search_queue_size = 128;

    /**
     * Size of the queue in the tree search maximal spaces algorithm for the pricing knapsack
     * subproblem of the sequential value correction algorithm.
     */
    NodeId sequential_value_correction_subproblem_tree_search_maximal_spaces_queue_size = 16;

    /**
     * Size of the queue in the tree search algorithm for the pricing knapsack subproblem of
     * the column generation algorithm.
     */
    NodeId column_generation_subproblem_tree_search_queue_size = 128;

    /**
     * Size of the queue in the tree search maximal spaces algorithm for the pricing knapsack
     * subproblem of the column generation algorithm.
     */
    NodeId column_generation_subproblem_tree_search_maximal_spaces_queue_size = 16;

    /*
     * Parameters for non-anytime mode
     */

    /** Size of the queue in the tree search algorithm. */
    NodeId not_anytime_tree_search_queue_size = 2048;

    /** Size of the queue in the tree search maximal spaces algorithm. */
    NodeId not_anytime_tree_search_maximal_spaces_queue_size = 16;

    /**
     * Size of the queue in the tree search algorithm for the single knapsack subproblem of
     * the sequential single knapsack algorithm.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = 2048;

    /**
     * Size of the queue in the tree search maximal spaces algorithm for the single knapsack
     * subproblem of the sequential single knapsack algorithm.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_tree_search_maximal_spaces_queue_size = 16;

    /** Number of iterations of the sequential value correction algorithm. */
    Counter not_anytime_sequential_value_correction_number_of_iterations = 32;

    /**
     * Size of the queue in the bin packing subproblem of the dichotomic search
     * algorithm.
     */
    NodeId not_anytime_dichotomic_search_subproblem_tree_search_queue_size = 128;
};

Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
