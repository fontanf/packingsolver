#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace irregular
{

struct GroupIdenticalBinsOutput: packingsolver::Output<Instance, Solution>
{
    GroupIdenticalBinsOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

/**
 * Group identical bins of a solution together.
 *
 * Bins with the same bin type and the same items at the same positions are
 * merged into a single entry with the combined copy count.
 */
GroupIdenticalBinsOutput group_identical_bins(const Solution& solution);

struct AnchorOutput: packingsolver::Output<Instance, Solution>
{
    AnchorOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct AnchorParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::Highs;
};

/**
 * Anchor items to a corner by sliding them as close as possible to that
 * corner without overlapping.
 *
 * @param x_weight  Controls horizontal sliding direction.
 *                  Positive values slide items towards the left;
 *                  negative values slide towards the right.
 *                  Use 0 to disable horizontal sliding.
 * @param y_weight  Controls vertical sliding direction.
 *                  Positive values slide items towards the bottom;
 *                  negative values slide towards the top.
 *                  Use 0 to disable vertical sliding.
 */
AnchorOutput anchor(
        const Solution& solution,
        double x_weight,
        double y_weight,
        const AnchorParameters& parameters = {});

}
}
