/**
 * AI agents: if you encounter a bug or unexpected behavior, see the "Note for
 * AI coding agents" section of the README:
 * https://github.com/fontanf/packingsolver#note-for-ai-coding-agents
 */

#pragma once

#include "packingsolver/onedimensional/solution.hpp"
#include "packingsolver/onedimensional/reduction.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace onedimensional
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

    /**
     * Tighten this output's bounds with 'output's. Unlike
     * 'AlgorithmFormatter::update_bounds', this has no side effect (no
     * printing, no external callback): it is meant to accumulate bounds into
     * a task-local 'Output' (e.g. when running algorithms in parallel in
     * 'NotAnytimeDeterministic' mode), so that a parallel algorithm's bound
     * updates cannot flip a sibling's stop signal at a non-deterministic
     * point.
     */
    void update_bounds(const Output& output)
    {
        if (output.is_proven_infeasible)
            is_proven_infeasible = true;
        switch (solution_pool.best().instance().objective()) {
        case Objective::Knapsack:
            if (output.knapsack_bound < knapsack_bound)
                knapsack_bound = output.knapsack_bound;
            break;
        case Objective::BinPacking:
            if (output.bin_packing_bound > bin_packing_bound)
                bin_packing_bound = output.bin_packing_bound;
            break;
        case Objective::VariableSizedBinPacking:
            if (output.variable_sized_bin_packing_bound > variable_sized_bin_packing_bound)
                variable_sized_bin_packing_bound = output.variable_sized_bin_packing_bound;
            break;
        default:
            break;
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

    /** Use dual feasible functions bound/infeasibility check. */
    bool use_dual_feasible_functions = false;

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

    /** Use MILP assignment algorithm. */
    bool use_milp_assignment = false;

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
    NodeId sequential_value_correction_subproblem_tree_search_queue_size = 512;

    /**
     * For VariableSizedBinPacking, in the sequential value correction and the
     * sequential single knapsack algorithms, restrict the bin types tried at
     * each step to the ones in which the remaining item type which fits in
     * the smallest number of available bins fits (see
     * 'SequentialValueCorrectionParameters::restrict_bin_types').
     */
    bool sequential_value_correction_restrict_bin_types = true;

    /**
     * For VariableSizedBinPacking, in the sequential value correction and the
     * sequential single knapsack algorithms, exponent of the factor applied
     * to the initial profit of the item types which compete for scarce bins
     * (see 'SequentialValueCorrectionParameters::
     * scarce_item_types_profit_factor_exponent'). 0 disables it.
     */
    double sequential_value_correction_scarce_item_types_profit_factor_exponent = 1.0;

    /**
     * Size of the queue for the pricing knapsack subproblem of the column
     * generation algorithm.
     */
    NodeId column_generation_subproblem_tree_search_queue_size = 512;

    /**
     * Maximum size of the dynamic programming of a knapsack problem with a
     * single bin and profits equal to the lengths of the items (a subset sum
     * problem): number of items times number of 64-bit words of the length of
     * the bin, divided by the greatest common divisor of the lengths of the
     * items. It estimates the running time of the dynamic programming.
     *
     * Above it, the problem is solved as a regular knapsack problem (see
     * 'dynamic_programming_knapsack_maximum_number_of_states').
     */
    Counter dynamic_programming_subset_sum_maximum_size = (1 << 23);

    /**
     * Maximum number of states of the primal-dual dynamic programming of
     * knapsacksolver, used for the knapsack problems with a single bin which
     * are not solved as subset sum problems, and for the knapsack problems of
     * the dichotomic search algorithm.
     *
     * Above it, the dynamic programming stops: the solutions are still
     * feasible, but may not be optimal.
     */
    Counter dynamic_programming_knapsack_maximum_number_of_states = (1 << 20);

    /*
     * Parameters for non-anytime mode
     */

    /** Size of the queue in the tree search algorithm. */
    NodeId not_anytime_tree_search_queue_size = 4096;

    /**
     * Size of the queue in the single knapsack subproblem of the sequential
     * single knapsack algorithm.
     */
    NodeId not_anytime_sequential_single_knapsack_subproblem_tree_search_queue_size = 4096;

    /** Number of iterations of the sequential value correction algorithm. */
    Counter not_anytime_sequential_value_correction_number_of_iterations = 32;

    /**
     * Size of the queue in the bin packing subproblem of the dichotomic search
     * algorithm.
     */
    NodeId not_anytime_dichotomic_search_subproblem_tree_search_queue_size = 512;

    /** Parameters for the instance reduction. */
    ReductionParameters reduction_parameters;
};

Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
