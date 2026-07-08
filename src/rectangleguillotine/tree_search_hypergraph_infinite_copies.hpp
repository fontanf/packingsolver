#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct TreeSearchHypergraphInfiniteCopiesOutput: packingsolver::Output<Instance, Solution>
{
    TreeSearchHypergraphInfiniteCopiesOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    /**
     * DP values indexed by width + (eff_width + 1) * height, where
     * eff_width  = bin_width  - left_trim - right_trim and
     * eff_height = bin_height - bottom_trim - top_trim.
     * dp_values[w + (eff_width + 1) * h] is the maximum profit achievable by
     * a guillotine packing (unbounded copies) into a w x h rectangle within
     * the trimmed area.
     */
    std::vector<Profit> dp_values;
};

struct TreeSearchHypergraphInfiniteCopiesParameters: packingsolver::Parameters<Instance, Solution>
{
};

const TreeSearchHypergraphInfiniteCopiesOutput tree_search_hypergraph_infinite_copies(
        const Instance& instance,
        const TreeSearchHypergraphInfiniteCopiesParameters& parameters = {});

}
}
