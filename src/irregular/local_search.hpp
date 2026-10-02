/**
 * Local search
 *
 * The goal of the local search algorithm is to improve a given solution by
 * iteratively applying local moves.
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

namespace packingsolver
{
namespace irregular
{

struct LocalSearchOutput: Output
{
    /** Constructor. */
    LocalSearchOutput(const Instance& instance):
        Output(instance) { }
};

struct LocalSearchParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** Seed for the random number generator. */
    Seed seed = 0;

    /**
     * Maximum number of iterations without improvement when packing an
     * item, after which the item is considered not to fit.
     *
     * -1 (the default) means no limit.
     */
    Counter maximum_number_of_iterations_without_improvement = -1;
};

/**
 * Run the local search algorithm for instances with objective Feasibility.
 */
LocalSearchOutput local_search(
        const Instance& instance,
        const LocalSearchParameters& parameters = {});

}
}
