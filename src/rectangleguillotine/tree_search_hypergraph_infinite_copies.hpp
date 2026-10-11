#pragma once

#include "packingsolver/rectangleguillotine/optimize.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct TreeSearchHypergraphInfiniteCopiesOutput: Output
{
    TreeSearchHypergraphInfiniteCopiesOutput(const Instance& instance):
        Output(instance) { }
};

struct TreeSearchHypergraphInfiniteCopiesParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /**
     * Maximum size of the array of the dynamic program (number of raster
     * points along x times along y). If the dynamic program with all the
     * item types needs a larger array, it only considers the item types of
     * largest profit per unit area whose array fits: with fewer item types,
     * there are much fewer raster points, and the solution is often almost
     * as good; but the bound is only computed with all the item types.
     *
     * Iterative scheme: maximum sizes 2^14, 2^16, 2^18..., up to this
     * maximum size, then all the item types if their array fits (if
     * negative: no maximum size).
     */
    Counter maximum_array_size = -1;
};

const TreeSearchHypergraphInfiniteCopiesOutput tree_search_hypergraph_infinite_copies(
        const Instance& instance,
        const TreeSearchHypergraphInfiniteCopiesParameters& parameters = {});

}
}
