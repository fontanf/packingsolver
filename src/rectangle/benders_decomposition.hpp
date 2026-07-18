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

    /** Maximum number of iterations. */
    Counter maximum_number_of_iterations = -1;

    /** Size of the queue for the knapsack subproblem. */
    NodeId subproblem_queue_size = 512;
};

BendersDecompositionOutput benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters);

}
}
