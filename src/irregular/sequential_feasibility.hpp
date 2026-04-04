/**
 * Sequential feasibility algorithm
 *
 * The algorithm iteratively solves bin packing sub-problems with a single bin
 * of decreasing size until no feasible packing is found. It is designed for
 * the OpenDimensionXY objective.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace irregular
{

struct SequentialFeasibilityOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    SequentialFeasibilityOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct SequentialFeasibilityParameters: packingsolver::Parameters<Instance, Solution>
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
};

SequentialFeasibilityOutput sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilityParameters& parameters = {});

}
}
