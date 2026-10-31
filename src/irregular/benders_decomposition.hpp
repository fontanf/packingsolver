/**
 * Benders decomposition
 *
 * Logic-based Benders decomposition, as the rectangle one (see
 * 'rectangle/benders_decomposition.hpp'), with a onedimensional master
 * problem:
 * - The master problem assigns the items to bins, each item having its area
 *   as length and each bin its area as capacity. Its bound is a bound of the
 *   instance as long as every cut added is valid.
 * - For each bin of the solution of the master problem, a subproblem checks
 *   whether its items fit in it. If they don't, a no-good cut forbids the
 *   minimal infeasible subsets of them in a bin of this type.
 * The algorithm stops when every bin of the solution of the master problem
 * is packed: this solution is then optimal.
 *
 * The subproblem is solved with the 'Feasibility' objective of 'optimize'.
 * It finds packings, but rarely proves that items don't fit, so the cuts are
 * then mostly not proven, and the bound of the master problem is not
 * reported. With 'subproblem_use_milp_nfp', the MILP NFP algorithm (see
 * 'milp_nfp.hpp') is first used to prove that the items don't fit; the cuts
 * it generates are proven.
 *
 * Supported objectives: 'Knapsack', 'BinPacking', 'VariableSizedBinPacking'.
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

namespace packingsolver
{
namespace irregular
{

struct BendersDecompositionOutput: Output
{
    /** Constructor. */
    BendersDecompositionOutput(const Instance& instance):
        Output(instance) { }

    /** Number of iterations. */
    Counter number_of_iterations = 0;

    /** Number of no-good cuts. */
    Counter number_of_no_good_cuts = 0;

    /** Number of no-good cuts which are not proven. */
    Counter number_of_unproven_no_good_cuts = 0;

    /** Total time spent building and solving the master problem. */
    double master_time = 0.0;

    /** Total time spent solving the subproblems. */
    double subproblem_time = 0.0;

    /** Total time spent enumerating minimal infeasible subsets. */
    double minimal_infeasible_subsets_time = 0.0;

    /** Number of item types lifted in the no-good cuts. */
    Counter number_of_lifted_coefficients = 0;

    /** Total time spent lifting the no-good cuts. */
    double lifting_time = 0.0;

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json["NumberOfIterations"] = number_of_iterations;
        json["NumberOfNoGoodCuts"] = number_of_no_good_cuts;
        json["NumberOfUnprovenNoGoodCuts"] = number_of_unproven_no_good_cuts;
        json["MasterTime"] = master_time;
        json["SubproblemTime"] = subproblem_time;
        json["MinimalInfeasibleSubsetsTime"] = minimal_infeasible_subsets_time;
        json["NumberOfLiftedCoefficients"] = number_of_lifted_coefficients;
        json["LiftingTime"] = lifting_time;
        return json;
    }

    virtual int format_width() const override
    {
        return 40;
    }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Number of iterations: " << number_of_iterations << std::endl
            << std::setw(width) << std::left << "Number of no-good cuts: " << number_of_no_good_cuts << std::endl
            << std::setw(width) << std::left << "Number of unproven no-good cuts: " << number_of_unproven_no_good_cuts << std::endl
            << std::setw(width) << std::left << "Master time (s): " << master_time << std::endl
            << std::setw(width) << std::left << "Subproblem time (s): " << subproblem_time << std::endl
            << std::setw(width) << std::left << "Minimal infeasible subsets time (s): " << minimal_infeasible_subsets_time << std::endl
            << std::setw(width) << std::left << "Number of lifted coefficients: " << number_of_lifted_coefficients << std::endl
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
     * Maximum number of iterations, in non-'Anytime' optimization modes only
     * (-1: unlimited).
     */
    Counter not_anytime_maximum_number_of_iterations = -1;

    /** Size of the queue of the tree search of the subproblems. */
    NodeId subproblem_queue_size = 512;

    /**
     * Use the MILP NFP algorithm to prove that the items of a bin don't fit.
     */
    bool subproblem_use_milp_nfp = false;

    /**
     * Maximum number of minimal infeasible subsets, and thus of no-good cuts,
     * per infeasible bin.
     *
     * Each subset costs many subproblem solves, which are expensive for
     * irregular items, so a single one is searched by default.
     */
    Counter maximum_number_of_no_good_cuts_per_bin = 1;

    /**
     * Maximum number of item types lifted in a no-good cut (see
     * 'lift_no_good_cut' in 'benders_decomposition.cpp'). Only with the MILP
     * NFP algorithm, which proves that the selections don't fit.
     */
    Counter maximum_number_of_lifted_item_types = 4;
};

BendersDecompositionOutput benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters);

}
}
