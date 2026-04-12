/**
 * Large-item-first algorithm
 *
 * A two-phase decomposition for instances with large item size differences.
 *
 * Phase 1: build a sub-instance containing only item types whose convex hull
 * area exceeds (max_non_fixed_convex_hull_area / 8) and solve it.
 *
 * Phase 2: fix the large items at the positions found in phase 1, then solve
 * the remaining (small) items in the resulting instance.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace irregular
{

struct LargeItemFirstOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    LargeItemFirstOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct LargeItemFirstParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;

    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Initial maximum approximation ratio (anytime mode). */
    double initial_maximum_approximation_ratio = 0.20;

    /** Multiplicative factor applied to the approximation ratio each iteration (anytime mode). */
    double maximum_approximation_ratio_factor = 0.75;

    /** Maximum approximation ratio (not-anytime mode). */
    double not_anytime_maximum_approximation_ratio = 0.05;

    /** Size of the queue in the tree search algorithm (not-anytime mode). */
    NodeId not_anytime_tree_search_queue_size = 512;

};

LargeItemFirstOutput large_item_first(
        const Instance& instance,
        const LargeItemFirstParameters& parameters = {});

}
}
