/**
 * MILP assignment
 *
 * The goal of the MILP assignment algorithm is to find a packing solution by
 * solving the classical assignment ("Kantorovich") mixed-integer linear
 * program of the Variable-sized Bin Packing Problem, or, for the (multiple)
 * Knapsack objective, of the Multiple Knapsack Problem.
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
     * Size of the tree search queue used to bound, for each bin type, the
     * number of bin instances of that type to consider in the MILP.
     *
     * Only used for the 'VariableSizedBinPacking' objective (for the
     * 'Knapsack' objective, the number of bin instances of each type is
     * given directly by the instance).
     */
    NodeId bin_count_subproblem_tree_search_queue_size = 1024;
};

MilpAssignmentOutput milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters = {});

}
}
