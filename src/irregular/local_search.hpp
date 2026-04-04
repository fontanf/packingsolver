/**
 * Local search
 *
 * The goal of the local search algorithm is to improve a given solution by
 * iteratively applying local moves.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

namespace packingsolver
{
namespace irregular
{

struct LocalSearchOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    LocalSearchOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct LocalSearchParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Seed for the random number generator. */
    Seed seed = 0;
};

/**
 * Run the local search algorithm for instances with objective Feasibility.
 */
LocalSearchOutput local_search(
        const Instance& instance,
        const LocalSearchParameters& parameters = {});

}
}
