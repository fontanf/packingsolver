#pragma once

#include "packingsolver/rectangle/solution.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace rectangle
{

struct Output: packingsolver::Output<Instance, Solution>
{
    Output(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    /** Knapsack bound. */
    Profit knapsack_bound = std::numeric_limits<Profit>::infinity();

    /** Bin packing bound. */
    BinPos bin_packing_bound = 0;

    /** Variable-sized bin packing bound. */
    Profit variable_sized_bin_packing_bound = 0;

    /** Open dimension X bound. */
    Length open_dimension_x_bound = 0;

    /** Open dimension Y bound. */
    Length open_dimension_y_bound = 0;

    /** True if the instance has been proven infeasible (Feasibility objective only). */
    bool is_proven_infeasible = false;

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = packingsolver::Output<Instance, Solution>::to_json();
        json["KnapsackBound"] = knapsack_bound;
        json["BinPackingBound"] = bin_packing_bound;
        json["VariableSizedBinPackingBound"] = variable_sized_bin_packing_bound;
        json["OpenDimensionXBound"] = open_dimension_x_bound;
        json["OpenDimensionYBound"] = open_dimension_y_bound;
        json["IsProvenInfeasible"] = is_proven_infeasible;
        return json;
    }

    virtual void format(std::ostream& os) const override
    {
        packingsolver::Output<Instance, Solution>::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Knapsack bound: " << knapsack_bound << std::endl
            << std::setw(width) << std::left << "Bin packing bound: " << bin_packing_bound << std::endl
            << std::setw(width) << std::left << "Variable-sized bin packing bound: " << variable_sized_bin_packing_bound << std::endl
            << std::setw(width) << std::left << "Open dimension X bound: " << open_dimension_x_bound << std::endl
            << std::setw(width) << std::left << "Open dimension Y bound: " << open_dimension_y_bound << std::endl
            << std::setw(width) << std::left << "Is proven infeasible: " << is_proven_infeasible << std::endl
            ;
    }
};

struct OptimizeParameters: packingsolver::Parameters<Instance, Solution, Output>
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

    /** Fixed items. */
    Solution* fixed_items = nullptr;

    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /** Use tree search algorithm. */
    bool use_tree_search = false;

    /** Use tree search maximal spaces algorithm. */
    bool use_tree_search_maximal_spaces = false;

    /** Use sequential single knapsack algorithm. */
    bool use_sequential_single_knapsack = false;

    /** Use sequential value correction algorithm. */
    bool use_sequential_value_correction = false;

    /** Use dichotomic search algorithm. */
    bool use_dichotomic_search = false;

    /** Use column generation algorithm. */
    bool use_column_generation = false;

    /** Use Benders decomposition algorithm. */
    bool use_benders_decomposition = false;

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

    /**
     * Size of the queue in the tree search algorithm for the pricing knapsack subproblem of
     * the Benders decomposition algorithm.
     */
    NodeId benders_decomposition_subproblem_tree_search_queue_size = 128;

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

    /** Number of iterations of the Benders decomposition algorithm. */
    Counter not_anytime_benders_decomposition_number_of_iterations = 32;

    /**
     * Size of the queue in the tree search algorithm for the bin packing subproblem of
     * the dichotomic search algorithm.
     */
    NodeId not_anytime_dichotomic_search_subproblem_tree_search_queue_size = 128;
};

Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
