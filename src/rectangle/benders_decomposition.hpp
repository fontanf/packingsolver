#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace rectangle
{

struct BendersDecompositionOutput: Output
{
    /** Constructor. */
    BendersDecompositionOutput(const Instance& instance):
        Output(instance) { }

    /** Number of iterations. */
    Counter number_of_iterations = 0;
};

struct BendersDecompositionParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;

    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Maximum number of iterations. */
    Counter maximum_number_of_iterations = -1;

    /** Size of the queue for the knapsack subproblem. */
    NodeId subproblem_queue_size = 512;

    /**
     * Maximum number of minimal-infeasible-subset no-good cuts to generate
     * per infeasible bin found (see 'enumerate_minimal_infeasible_subsets'
     * in 'benders_decomposition.cpp'); bounds how much the search for
     * additional cuts can cost per Benders iteration.
     */
    Counter maximum_number_of_no_good_cuts_per_bin = 8;
};

BendersDecompositionOutput benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters);

}
}
