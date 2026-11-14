#pragma once

#include "packingsolver/rectangleguillotine/optimize.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct BendersDecompositionOutput: Output
{
    /** Constructor. */
    BendersDecompositionOutput(const Instance& instance):
        Output(instance) { }

    /** Number of iterations. */
    Counter number_of_iterations = 0;

    /**
     * Number of iterations that stopped early because some bin of the
     * master's candidate had a dual-feasible-function cut violation (see
     * Pass 1 in 'benders_decomposition'), skipping every later pass that
     * iteration.
     */
    Counter number_of_dual_feasible_function_terminations = 0;

    /**
     * Number of iterations that stopped early because some bin of the
     * master's candidate had an incompatible-triplet cut violation (see
     * Pass 1b in 'benders_decomposition'), skipping the feasibility
     * subproblem that iteration.
     */
    Counter number_of_triplet_terminations = 0;

    /**
     * Number of iterations where the feasibility subproblem found at
     * least one infeasible bin (see Pass 2 in 'benders_decomposition'),
     * adding one or more no-good cuts instead of converging that
     * iteration.
     */
    Counter number_of_subproblem_terminations = 0;

    /** Total time spent building and solving the master problem. */
    double master_time = 0.0;

    /** Total time spent searching for dual-feasible-function cuts. */
    double dual_feasible_functions_time = 0.0;

    /** Total time spent searching for incompatible triplets. */
    double triplets_time = 0.0;

    /** Total time spent solving feasibility subproblems. */
    double subproblem_time = 0.0;

    /** Total time spent enumerating minimal infeasible subsets. */
    double minimal_infeasible_subsets_time = 0.0;

    /** Total time spent lifting no-good cuts. */
    double lifting_time = 0.0;

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json["NumberOfIterations"] = number_of_iterations;
        json["NumberOfDualFeasibleFunctionTerminations"] = number_of_dual_feasible_function_terminations;
        json["NumberOfTripletTerminations"] = number_of_triplet_terminations;
        json["NumberOfSubproblemTerminations"] = number_of_subproblem_terminations;
        json["MasterTime"] = master_time;
        json["DualFeasibleFunctionsTime"] = dual_feasible_functions_time;
        json["TripletsTime"] = triplets_time;
        json["SubproblemTime"] = subproblem_time;
        json["MinimalInfeasibleSubsetsTime"] = minimal_infeasible_subsets_time;
        json["LiftingTime"] = lifting_time;
        return json;
    }

    virtual int format_width() const override
    {
        // Fits "Number of dual feasible function terminations: ", the
        // longest of the lines printed below.
        return 48;
    }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Number of iterations: " << number_of_iterations << std::endl
            << std::setw(width) << std::left << "Number of dual feasible function terminations: " << number_of_dual_feasible_function_terminations << std::endl
            << std::setw(width) << std::left << "Number of triplet terminations: " << number_of_triplet_terminations << std::endl
            << std::setw(width) << std::left << "Number of subproblem terminations: " << number_of_subproblem_terminations << std::endl
            << std::setw(width) << std::left << "Master time (s): " << master_time << std::endl
            << std::setw(width) << std::left << "Dual feasible functions time (s): " << dual_feasible_functions_time << std::endl
            << std::setw(width) << std::left << "Triplets time (s): " << triplets_time << std::endl
            << std::setw(width) << std::left << "Subproblem time (s): " << subproblem_time << std::endl
            << std::setw(width) << std::left << "Minimal infeasible subsets time (s): " << minimal_infeasible_subsets_time << std::endl
            << std::setw(width) << std::left << "Lifting time (s): " << lifting_time << std::endl
            ;
    }
};

struct BendersDecompositionParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** Linear programming solver of the master problem and subproblems. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /**
     * Use the tree search algorithm to solve the master problem.
     *
     * If neither 'master_problem_use_tree_search' nor
     * 'master_problem_use_milp_assignment' is set, the algorithms solving the
     * master problem are selected automatically.
     */
    bool master_problem_use_tree_search = false;

    /** Use the MILP assignment algorithm to solve the master problem. */
    bool master_problem_use_milp_assignment = false;

    /**
     * Maximum number of iterations, in non-'Anytime' optimization modes
     * only (-1: unlimited). 'Anytime' mode instead relies on the timer (or
     * full convergence, i.e. every bin of the master's candidate found
     * geometrically feasible) to stop, since it is expected to keep
     * improving for as long as it is given to run.
     */
    Counter not_anytime_maximum_number_of_iterations = -1;

    /**
     * Maximum number of iterations that actually solve at least one
     * feasibility subproblem, in non-'Anytime' optimization modes only
     * (-1: unlimited). Unlike 'not_anytime_maximum_number_of_iterations',
     * an iteration whose master candidate is cut by a dual-feasible-
     * function inequality before any subproblem is solved (see
     * 'find_most_violated_dual_feasible_function_cut' in
     * 'benders_decomposition.cpp') doesn't count towards this limit, since
     * it never actually pays for a subproblem solve.
     */
    Counter not_anytime_maximum_number_of_subproblem_solves = 100;

    /**
     * Maximum number of minimal-infeasible-subset no-good cuts to generate
     * per infeasible bin found (see 'enumerate_minimal_infeasible_subsets'
     * in 'benders_decomposition.cpp'); bounds how much the search for
     * additional cuts can cost per Benders iteration.
     */
    Counter maximum_number_of_no_good_cuts_per_bin = 8;
};

/**
 * Return 'true' iff the Benders decomposition algorithm supports the
 * instance.
 *
 * It supports the 'Knapsack', 'BinPacking' and 'VariableSizedBinPacking'
 * objectives, without stacks of several item types, and with the penalized
 * resources which count the copies of item types up to a threshold (a
 * non-negative integer capacity, and consumption schedules '(1, ..., 1, 0)'):
 * the other ones are not supported by the master problem.
 */
bool benders_decomposition_supported(const Instance& instance);

/**
 * Benders decomposition algorithm.
 *
 * A copy of the rectangle Benders decomposition (see
 * 'rectangle/benders_decomposition.hpp'), whose master problem and cuts are
 * built on the non-guillotine relaxation of the instance: every guillotine
 * packing of a bin is a packing of the bin without its hard trims. The
 * feasibility
 * subproblems are guillotine: each one checks whether the items that the
 * master problem assigns to a bin have a guillotine packing in this bin.
 *
 * On an instance it doesn't support, it returns immediately.
 */
const BendersDecompositionOutput benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters = {});

}
}
