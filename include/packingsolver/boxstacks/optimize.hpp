#pragma once

#include "packingsolver/boxstacks/solution.hpp"
#include "packingsolver/boxstacks/reduction.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace boxstacks
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

    /** True if the instance has been proven infeasible. */
    bool is_proven_infeasible = false;

    /** Return 'true' iff the current bound proves the best solution optimal. */
    bool is_proven_optimal() const
    {
        // A proven-infeasible instance is done being searched regardless of
        // objective: there is no better bound to wait for.
        if (is_proven_infeasible)
            return true;
        switch (solution_pool.best().instance().objective()) {
        case Objective::Knapsack:
            return solution_pool.best().feasible()
                && equal_profit(knapsack_bound, solution_pool.best().profit());
        case Objective::BinPacking:
            return solution_pool.best().feasible()
                && bin_packing_bound == solution_pool.best().number_of_bins();
        case Objective::VariableSizedBinPacking:
            return solution_pool.best().feasible()
                && equal_cost(variable_sized_bin_packing_bound, solution_pool.best().cost());
        case Objective::Feasibility:
            return solution_pool.best().feasible();
        default:
            return false;
        }
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = packingsolver::Output<Instance, Solution>::to_json();
        json["KnapsackBound"] = knapsack_bound;
        json["BinPackingBound"] = bin_packing_bound;
        json["VariableSizedBinPackingBound"] = variable_sized_bin_packing_bound;
        json["IsProvenInfeasible"] = is_proven_infeasible;
        return json;
    }

    virtual void format(std::ostream& os) const override
    {
        packingsolver::Output<Instance, Solution>::format(os);
        int width = format_width();
        os << std::setw(width) << std::left << "Is proven infeasible: " << is_proven_infeasible << std::endl;
        switch (solution_pool.best().instance().objective()) {
        case Objective::Knapsack:
            write_bound(os, width, solution_pool.best().profit(), knapsack_bound);
            break;
        case Objective::BinPacking:
            write_bound(os, width, solution_pool.best().number_of_bins(), bin_packing_bound);
            break;
        case Objective::VariableSizedBinPacking:
            write_bound(os, width, solution_pool.best().cost(), variable_sized_bin_packing_bound);
            break;
        case Objective::Feasibility:
            break;
        default:
            break;
        }
    }


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

    bool sequential_onedimensional_rectangle_failed = false;

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

    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /**
     * Compute a bound on the 'box' relaxation (trivial, 1D and dual
     * feasible functions bounds) for the 'Knapsack', 'BinPacking' and
     * 'VariableSizedBinPacking' objectives, before the primal algorithms.
     */
    bool use_box_bounds = true;

    /** Use sequential single knapsack algorithm. */
    bool use_sequential_single_knapsack = false;

    /** Use sequential value correction algorithm. */
    bool use_sequential_value_correction = false;

    /** Guides used in the tree search algorithm. */
    std::vector<GuideId> tree_search_guides;

    /**
     * Threshold to consider that a bin contains "many" items - see the
     * automatic selection between 'use_sequential_single_knapsack' and
     * 'use_sequential_value_correction' for multi-bin 'BinPacking' instances
     * in 'optimize()'.
     */
    Counter many_items_in_bins_threshold = 16;

    /** Threshold to consider that a bin contains "many" items. */
    Counter many_items_in_bins_threshold_2 = 64;

    /** Factor to consider that the number of copies of items is "high". */
    Counter many_item_type_copies_factor = 1;

    /**
     * Size of the queue for the pricing knapsack subproblem of the sequential
     * value correction algorithm.
     */
    NodeId sequential_value_correction_subproblem_tree_search_queue_size = 512;

    /**
     * Size of the queue for the pricing knapsack subproblem of the column
     * generation algorithm.
     */
    NodeId column_generation_subproblem_tree_search_queue_size = 512;

    /*
     * Parameters for anytime mode
     */

    /**
     * Initial size of the queue of the boxstacks branching scheme's 3D
     * fallback inside the sequential_onedimensional_rectangle algorithm,
     * grown from there - see 'optimize_sequential_onedimensional_rectangle()'.
     */
    NodeId anytime_tree_search_initial_queue_size = 1;

    /**
     * Initial size of the queue of the rectangle subproblem inside the
     * sequential_onedimensional_rectangle algorithm, grown from there - see
     * 'optimize_sequential_onedimensional_rectangle()'.
     *
     * Twice 'anytime_tree_search_initial_queue_size' by default, matching
     * the ratio between 'not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size'
     * (1024) and 'not_anytime_tree_search_queue_size' (512), so that both
     * queues reach their respective non-anytime size at the same level as
     * they grow together, instead of one finishing growing well before the
     * other.
     */
    NodeId anytime_sequential_onedimensional_rectangle_rectangle_initial_queue_size = 2;

    /*
     * Parameters for non-anytime mode
     */

    /** Size of the queue in the tree search algorithm. */
    NodeId not_anytime_tree_search_queue_size = 512;

    /**
     * Size of the queue of the boxstacks branching scheme's 3D fallback
     * inside the single knapsack subproblem of the sequential single
     * knapsack algorithm (each subproblem call recurses into a single-bin
     * 'optimize()', which goes through
     * 'optimize_sequential_onedimensional_rectangle()' - see there).
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = 512;

    /**
     * Size of the queue of the rectangle subproblem inside the single
     * knapsack subproblem of the sequential single knapsack algorithm - see
     * 'not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size'
     * above.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_rectangle_tree_search_queue_size = 1024;

    /**
     * Size of the queue of the rectangle subproblem inside the
     * sequential_onedimensional_rectangle algorithm - see
     * 'optimize_sequential_onedimensional_rectangle()'. The boxstacks
     * branching scheme's own 3D fallback there uses
     * 'not_anytime_tree_search_queue_size' instead.
     */
    NodeId not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size = 1024;

    /** Number of iterations of the sequential value correction algorithm. */
    Counter not_anytime_sequential_value_correction_number_of_iterations = 32;

    /** Parameters for the instance reduction. */
    ReductionParameters reduction_parameters;
};

Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
