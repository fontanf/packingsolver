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

    /** Maximum approximation ratio (not-anytime mode). */
    double not_anytime_maximum_approximation_ratio = 0.05;

    /** Size of the queue in the tree search algorithm (not-anytime mode). */
    NodeId not_anytime_tree_search_queue_size = 512;

    /** Use tree search in sub-problems. */
    bool use_tree_search = false;

    /** Use local search in sub-problems. */
    bool use_local_search = false;

    /** Use MILP raster in sub-problems. */
    bool use_milp_raster = false;

    /** Use sequential single knapsack in sub-problems. */
    bool use_sequential_single_knapsack = false;

    /** Use sequential value correction in sub-problems. */
    bool use_sequential_value_correction = false;

    /** Use dichotomic search in sub-problems. */
    bool use_dichotomic_search = false;

    /** Use column generation in sub-problems. */
    bool use_column_generation = false;

    /** Use sequential feasibility in sub-problems. */
    bool use_sequential_feasibility = false;
};

LargeItemFirstOutput large_item_first(
        const Instance& instance,
        const LargeItemFirstParameters& parameters = {});

}
}
