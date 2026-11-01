/**
 * Trivial bound
 *
 * Compute cheap bounds based on the lengths of the items and of the bins:
 * - Knapsack: continuous relaxation with the lengths of all the bins pooled
 *   together;
 * - BinPacking: number of bins, in the order of the instance, needed to
 *   cover the total length of the items;
 * - VariableSizedBinPacking: minimum cost of a selection of bins whose total
 *   length covers the total length of the items (a knapsack problem solved by
 *   dynamic programming).
 *
 * No bound is computed for the other objectives.
 *
 * These bounds don't depend on how the items can be combined in the bins, so
 * they remain cheap whatever the number of item types. They are used by the
 * other problem types when the full onedimensional relaxation would be too
 * expensive to solve.
 */

#pragma once

#include "packingsolver/onedimensional/optimize.hpp"

namespace packingsolver
{
namespace onedimensional
{

struct TrivialBoundParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /**
     * Linear programming solver (see
     * 'OptimizeParameters::linear_programming_solver_name').
     */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /**
     * Maximum size of the dynamic programming of a subset sum problem (see
     * 'OptimizeParameters::dynamic_programming_subset_sum_maximum_size').
     */
    Counter dynamic_programming_subset_sum_maximum_size = (1 << 23);

    /**
     * Maximum number of states of the primal-dual dynamic programming (see
     * 'OptimizeParameters::dynamic_programming_knapsack_maximum_number_of_states').
     */
    Counter dynamic_programming_knapsack_maximum_number_of_states = (1 << 20);
};

Output trivial_bound(
        const Instance& instance,
        const TrivialBoundParameters& parameters);

}
}
