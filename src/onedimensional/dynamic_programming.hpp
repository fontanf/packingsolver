/**
 * Dynamic programming
 *
 * Solve knapsack problems with a single bin by dynamic programming.
 *
 * The items are converted into the items of a 0-1 knapsack problem by binary
 * decomposition of their copies.
 *
 * If the profit of each item is equal to its length (minus its nesting
 * length), the problem is a subset sum problem. If it is small enough, it is
 * solved with the word RAM dynamic programming of subsetsumsolver. Otherwise,
 * it is solved with the primal-dual dynamic programming of knapsacksolver.
 */

#pragma once

#include "packingsolver/onedimensional/optimize.hpp"

namespace packingsolver
{
namespace onedimensional
{

struct DynamicProgrammingOutput: Output
{
    /** Constructor. */
    DynamicProgrammingOutput(const Instance& instance):
        Output(instance) { }
};

struct DynamicProgrammingParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /**
     * Maximum size of the dynamic programming of a subset sum problem (see
     * 'OptimizeParameters::dynamic_programming_subset_sum_maximum_size').
     */
    Counter subset_sum_maximum_size = (1 << 23);

    /**
     * Maximum number of states of the primal-dual dynamic programming (see
     * 'OptimizeParameters::dynamic_programming_knapsack_maximum_number_of_states').
     */
    Counter knapsack_maximum_number_of_states = (1 << 20);
};

DynamicProgrammingOutput dynamic_programming(
        const Instance& instance,
        const DynamicProgrammingParameters& parameters);

}
}
