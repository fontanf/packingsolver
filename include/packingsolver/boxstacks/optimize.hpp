#pragma once

#include "packingsolver/boxstacks/solution.hpp"

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

    /** True if the instance has been proven infeasible (Feasibility objective only). */
    bool is_proven_infeasible = false;

    /** Return 'true' iff the current bound proves the best solution optimal. */
    bool is_proven_optimal() const
    {
        switch (solution_pool.best().instance().objective()) {
        case Objective::Knapsack:
            return equal(knapsack_bound, solution_pool.best().profit());
        case Objective::BinPacking:
            return solution_pool.best().full()
                && bin_packing_bound == solution_pool.best().number_of_bins();
        case Objective::VariableSizedBinPacking:
            return solution_pool.best().full()
                && equal(variable_sized_bin_packing_bound, solution_pool.best().cost());
        case Objective::Feasibility:
            return (solution_pool.best().full()
                    && solution_pool.best().feasible())
                || is_proven_infeasible;
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
        switch (solution_pool.best().instance().objective()) {
        case Objective::Knapsack:
            os << std::setw(width) << std::left << "Knapsack bound: " << knapsack_bound << std::endl;
            break;
        case Objective::BinPacking:
            os << std::setw(width) << std::left << "Bin packing bound: " << bin_packing_bound << std::endl;
            break;
        case Objective::VariableSizedBinPacking:
            os << std::setw(width) << std::left << "Variable-sized bin packing bound: " << variable_sized_bin_packing_bound << std::endl;
            break;
        case Objective::Feasibility:
            os << std::setw(width) << std::left << "Is proven infeasible: " << is_proven_infeasible << std::endl;
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

    /** Use tree search algorithm. */
    bool use_tree_search = false;

    /** Use sequential single knapsack algorithm. */
    bool use_sequential_single_knapsack = false;

    /** Guides used in the tree search algorithm. */
    std::vector<GuideId> tree_search_guides;

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
     * Parameters for non-anytime mode
     */

    /** Size of the queue in the tree search algorithm. */
    NodeId not_anytime_tree_search_queue_size = 512;

    /**
     * Size of the queue in the single knapsack subproblem of the sequential
     * single knapsack algorithm.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = 512;

    /** Number of iterations of the sequential value correction algorithm. */
    Counter not_anytime_sequential_value_correction_number_of_iterations = 32;
};

Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
