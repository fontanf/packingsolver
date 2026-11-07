#pragma once

#include "packingsolver/rectangleguillotine/optimize.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct TreeSearchHypergraphOutput: Output
{
    TreeSearchHypergraphOutput(const Instance& instance):
        Output(instance) { }

    /** Number of searches (one for each threshold). */
    Counter number_of_searches = 0;

    /** Number of forward labels kept by the last search. */
    Counter number_of_forward_labels = 0;

    /** Number of backward labels kept by the last search. */
    Counter number_of_backward_labels = 0;
};

struct TreeSearchHypergraphParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /**
     * Maximum number of states of the hypergraph. If the hypergraph of the
     * instance has more states, the algorithm stops immediately (if
     * negative: no maximum).
     */
    Counter maximum_number_of_states = -1;

    /**
     * Use the Lagrangian relaxation of the numbers of copies of the item
     * types for the bounds, with the multipliers of the linear relaxation of
     * the flow formulation (computed by column generation). Otherwise, the
     * bounds ignore the numbers of copies.
     */
    bool use_lagrangian_relaxation = true;

    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /**
     * Function returning the profit of the best solution known, for example
     * found by other algorithms running in parallel. The thresholds are never
     * below it.
     */
    std::function<Profit()> best_known_profit = []() { return 0.0; };
};

/**
 * Return 'true' iff the tree search hypergraph algorithm supports the
 * instance: it then finds an optimal solution, and proves it.
 *
 * On another instance, the features it doesn't support are ignored; the
 * solutions which don't satisfy them are not returned. The bounds are only
 * returned with a single bin.
 *
 * It supports the 'Knapsack' and 'Feasibility' objectives with a single bin,
 * any number of stages (or an unlimited number of stages), the 'NonExact',
 * 'Exact' and 'Roadef2018' cut types (any cut type with an unlimited number
 * of stages), any first stage orientation, the cut thickness, the item
 * rotations, the hard trims and the soft right and top trims.
 */
bool tree_search_hypergraph_supported(const Instance& instance);

/**
 * Get the number of states of the hypergraph of an instance supported by the
 * tree search hypergraph algorithm.
 */
Counter tree_search_hypergraph_number_of_states(const Instance& instance);

/**
 * Tree search hypergraph algorithm for the guillotine 2-dimensional knapsack
 * problem with a single bin.
 *
 * Bidirectional labeling algorithm in a decision hypergraph, with the
 * b-value as resource (Léonard and Clautiaux, 2026).
 *
 * The states of the hypergraph are the sub-plates of the patterns (with their
 * stage), and its hyperarcs are the guillotine cuts. A label of a state is
 * the vector of the numbers of copies of each item type of a pattern of the
 * state. The forward labels of a state are built from the forward labels of
 * the sources of its hyperarcs, the backward labels from the backward labels
 * of the targets and the forward labels of the other sources. A complete
 * pattern is a forward label of the bin, or the merge of the forward labels
 * of the sources of a hyperarc with a backward label of its target.
 *
 * For a threshold, the search returns all the patterns of profit at least
 * this threshold. The threshold is decreased until the best pattern found
 * reaches it.
 */
const TreeSearchHypergraphOutput tree_search_hypergraph(
        const Instance& instance,
        const TreeSearchHypergraphParameters& parameters = {});

}
}
