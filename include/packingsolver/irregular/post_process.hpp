#pragma once

#include "packingsolver/irregular/optimize.hpp"

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

/**
 * Re-optimize the bin at 'bin_pos' of a solution, as its own single-bin
 * BinPackingWithLeftovers instance, with 'optimize' and 'parameters' (its
 * output and its post-process being disabled).
 *
 * Return the solution with that bin replaced by the re-optimized one, or the
 * solution itself if the re-optimization didn't pack all the items of the
 * bin.
 */
Solution reoptimize_bin(
        const Solution& solution,
        BinPos bin_pos,
        const OptimizeParameters& parameters = {});

/**
 * Re-optimize each bin of a solution except the last one (see
 * 'reoptimize_bin'), and keep a re-optimized bin if its leftover value is
 * greater, or if its leftover value is the same and its staircase leftover
 * value is greater.
 *
 * With the algorithms selected as for any instance, a bin with many items
 * found quickly, for example by the tree search periodic packing, is
 * re-optimized quickly too.
 *
 * The leftover values of the other bins only come after the ones of the last
 * bin in the objective, so the algorithms hardly try to keep the free space
 * of the other bins in a single region.
 */
Solution reoptimize_non_last_bins(
        const Solution& solution,
        const OptimizeParameters& parameters = {});

}
}
