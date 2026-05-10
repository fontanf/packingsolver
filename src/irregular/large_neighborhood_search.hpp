#pragma once

#include "packingsolver/irregular/solution.hpp"

namespace packingsolver
{
namespace irregular
{

struct LargeNeighborhoodSearchParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Number of rectangle candidates evaluated to select the best one per iteration. */
    Counter number_of_rectangle_candidates = 10;

    /** Minimum ratio of the longer side to the shorter side of the selected rectangle. */
    double minimum_aspect_ratio = 3.0;

    /** Multiplicative factor applied to profits of item types with unplaced copies after each iteration. */
    double profit_multiplier = 1.5;

    /** Queue size for the sub-problem tree search. */
    NodeId subproblem_queue_size = 512;
};

struct LargeNeighborhoodSearchOutput: packingsolver::Output<Instance, Solution>
{
    LargeNeighborhoodSearchOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

const LargeNeighborhoodSearchOutput large_neighborhood_search(
        const Instance& instance,
        const LargeNeighborhoodSearchParameters& parameters = {});

}
}
