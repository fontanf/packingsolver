#pragma once

#include "packingsolver/rectangle/solution.hpp"

namespace packingsolver
{
namespace rectangle
{

struct BendersDecompositionOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    BendersDecompositionOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }


    /** Number of iterations. */
    Counter number_of_iterations = 0;
};

struct BendersDecompositionParameters: packingsolver::Parameters<Instance, Solution>
{
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
