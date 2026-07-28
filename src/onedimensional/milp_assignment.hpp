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

    /**
     * Size of the tree search queue used:
     * - for 'VariableSizedBinPacking', to bound, for each bin type, the
     *   number of bin instances of that type to consider in the MILP;
     * - for 'BinPacking', for a quick tree search pass over the whole
     *   instance itself, ahead of the (potentially slower) MILP.
     * Not used for 'Knapsack' or 'Feasibility'.
     */
    NodeId bin_count_subproblem_tree_search_queue_size = 1024;
};

MilpAssignmentOutput milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters = {});

}
}
