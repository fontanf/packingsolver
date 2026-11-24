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
 * Re-optimize the leftovers of the bin at 'bin_pos' of a solution, by solving
 * that bin as its own single-bin BinPackingWithLeftovers instance, with
 * 'optimize' and 'parameters' (its output and its post-process being
 * disabled).
 *
 * Each time the re-optimization finds a better solution of the bin, the
 * solution with that bin replaced is passed to
 * 'parameters.new_solution_callback', in an output of the instance of
 * 'solution'. It isn't compared with 'solution': its leftovers may be worse.
 *
 * Return the solution with that bin replaced by the re-optimized one, or the
 * solution itself if the bin is empty or if the re-optimization didn't pack
 * all the items of the bin.
 */
Solution reoptimize_bin_leftovers(
        const Solution& solution,
        BinPos bin_pos,
        const OptimizeParameters& parameters = {});

/**
 * Re-optimize the leftovers of each bin of a solution except the last one
 * (see 'reoptimize_bin_leftovers'), and keep a re-optimized bin if its
 * leftover value is greater, or if its leftover value is the same and its
 * staircase leftover value is greater (see 'Solution::leftovers_better').
 *
 * 'parameters.new_solution_callback' isn't called.
 *
 * With the algorithms selected as for any instance, a bin with many items
 * found quickly is re-optimized quickly too.
 *
 * The leftover values of the other bins only come after the ones of the last
 * bin in the objective, so the algorithms hardly try to keep the free space
 * of the other bins in a single region.
 */
Solution reoptimize_non_last_bins_leftovers(
        const Solution& solution,
        const OptimizeParameters& parameters = {});

/**
 * Re-optimize the leftovers of each bin of a solution (see
 * 'reoptimize_non_last_bins_leftovers').
 *
 * It is used by the leftover post-process for the objectives other than
 * 'BinPackingWithLeftovers', for which the last bin plays no special role.
 */
Solution reoptimize_all_bins_leftovers(
        const Solution& solution,
        const OptimizeParameters& parameters = {});

}
}
