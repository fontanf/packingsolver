/**
 * MILP assignment
 *
 * The goal of the MILP assignment algorithm is to find a packing solution by
 * solving the classical assignment ("Kantorovich") mixed-integer linear
 * program of the Variable-sized Bin Packing Problem, or, for the (multiple)
 * Knapsack objective, of the Multiple Knapsack Problem, or, for the
 * Feasibility objective, of the corresponding feasibility problem (packing
 * all the items into the instance's fixed set of bins), or, for the
 * BinPacking objective, of the Bin Packing Problem where, if there are
 * multiple bin types, they must be used in the order they are provided.
 */

#pragma once

#include "packingsolver/onedimensional/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace onedimensional
{

struct MilpAssignmentOutput: Output
{
    /** Constructor. */
    MilpAssignmentOutput(const Instance& instance):
        Output(instance) { }

};

struct MilpAssignmentParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;

    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /**
     * Size of the tree search queue used:
     * - for 'VariableSizedBinPacking', to bound, for each bin type, the
     *   number of bin instances of that type to consider in the MILP;
     * - for 'BinPacking', for a quick tree search pass over the whole
     *   instance itself, ahead of the (potentially slower) MILP.
     * Not used for 'Knapsack' or 'Feasibility'.
     */
    NodeId bin_count_subproblem_tree_search_queue_size = 1024;

    /**
     * For the 'BinPacking' objective, solve as a sequence of 'Feasibility'
     * sub-problems (one per candidate bin count, packing all items into a
     * fixed number of bins) instead of a single combined MILP with
     * bin-activation ('y') variables spanning the whole candidate bin
     * range.
     *
     * The tree search pass ahead of the MILP (see
     * 'bin_count_subproblem_tree_search_queue_size') already provides a
     * lower bound 'lb'. Candidate bin counts 'lb + 1, lb + 2, ...' are then
     * tried in increasing order, each as a 'Feasibility' MILP packing all
     * items into that many bins (in bin type order); the search stops at
     * the first one found feasible ('lb' itself is not tried; a solution
     * using exactly 'lb' bins, if one exists, is not found by this
     * scheme). If the tree search pass already found a full solution,
     * candidates are only tried strictly below its own bin count (trying
     * it would be redundant, it is already known feasible); once every
     * candidate below it has been shown infeasible, that solution is kept
     * and proven optimal. Otherwise, candidates keep increasing until one
     * is found feasible or the timer ends.
     *
     * Not used for other objectives.
     */
    bool use_sequential_feasibility = true;
};

/**
 * 'lower_bound': a known lower bound on the number of bins already
 * established by the caller (e.g. a bound computed by another algorithm
 * before this one runs), used only by the 'use_sequential_feasibility'
 * scheme to seed its starting candidate bin count instead of always
 * starting from scratch. Combined (via 'max') with 'output.bin_packing_bound'
 * itself, so it is never unsound to pass a stale or overly conservative
 * value.
 */
MilpAssignmentOutput milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters = {},
        BinPos lower_bound = 0);

}
}
