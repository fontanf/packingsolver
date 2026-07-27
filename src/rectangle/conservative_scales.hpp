/**
 * Conservative scales
 *
 * Lower bound for bin packing problems (with a single bin type and without
 * item rotation) obtained by jointly rescaling item widths and heights,
 * based on which items can share a row or a column of the bin.
 *
 * Unlike the per-axis dual feasible functions of 'dual_feasible_functions.hpp'
 * (which reason about widths and heights independently), this bound can
 * prove tighter results when no per-axis argument suffices, at a higher
 * (but still polynomial per iteration) computational cost.
 *
 * Reference:
 * - "One-dimensional relaxations and LP bounds for orthogonal packing"
 *   (Belov, Kartak, Rohling & Scheithauer, 2013)
 *   https://doi.org/10.1007/s10288-012-0206-8
 * - as used in "A Primal Decomposition Algorithm for the Two-dimensional Bin
 *   Packing Problem" / "Combinatorial Benders Decomposition for the
 *   Two-Dimensional Bin Packing Problem" (Côté, Haouari & Iori, 2019/2021),
 *   Section 5 ('L_BKRS').
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

namespace packingsolver
{
namespace rectangle
{

struct ConservativeScalesOutput: Output
{
    /** Constructor. */
    ConservativeScalesOutput(const Instance& instance):
        Output(instance) { }
};

struct ConservativeScalesParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /**
     * Number of alternating width/height rescaling iterations ('eta' in the
     * reference paper). Each iteration adds two more candidate rescaled
     * dimension vectors, contributing (iteration + 1)^2 new area-bound
     * combinations - the paper does not specify a concrete value, this
     * default is our own choice.
     */
    Counter number_of_iterations = 5;
};

/**
 * Compute the 'L_BKRS' ("conservative scales") lower bound.
 *
 * Only meaningful for the 'BinPacking' and 'Feasibility' objectives (a pure
 * bin-count argument; unlike 'dual_feasible_functions', it has no
 * established profit-bound analogue for 'Knapsack'), only for instances with
 * a single bin type, and only if every item type is oriented (see
 * 'Instance::all_item_types_oriented').
 *
 * This last requirement matters: 'conservative_scales' assumes a fixed
 * orientation for every item, matching the 2D-BPP variant the underlying
 * paper targets ("items... cannot be rotated"). Treating a non-oriented item
 * as fixed at its stored (width, height) can derive a rescaling that is
 * unsound (invalid, possibly overstating the true minimum bin count)
 * whenever that item could only be validly grouped with others via its
 * rotated orientation.
 */
ConservativeScalesOutput conservative_scales(
        const Instance& instance,
        const ConservativeScalesParameters& parameters);

}
}
