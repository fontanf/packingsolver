#pragma once

#include "packingsolver/box/optimize.hpp"

namespace packingsolver
{
namespace box
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
 * found quickly is re-optimized quickly too.
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
