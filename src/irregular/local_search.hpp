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
 * Run the appropriate local search algorithm depending on the objective of
 * the instance (BinPacking → local_search_bin_packing, OpenDimensionX →
 * local_search_open_dimension_x).
 */
LocalSearchOutput local_search(
        const Instance& instance,
        const LocalSearchParameters& parameters = {});

}
}
