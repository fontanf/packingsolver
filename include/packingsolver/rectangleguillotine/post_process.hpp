#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct GroupIdenticalBinsOutput: packingsolver::Output<Instance, Solution>
{
    GroupIdenticalBinsOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

/**
 * Group identical bins of a solution together.
 *
 * Bins with the same bin type and the same guillotine cut tree are merged into
 * a single entry with the combined copy count.
 */
GroupIdenticalBinsOutput group_identical_bins(const Solution& solution);

/**
 * Represents one item placed at a known position in a bin.
 */
struct ItemPlacement
{
    ItemTypeId item_type_id;
    Length l, r, b, t;
};

/**
 * Given a set of items with known positions in a bin, attempt to reconstruct
 * one or two minimal-stage guillotine-cut Solutions.
 *
 * Returns an empty vector if the placement is not guillotine-cuttable.
 *
 * If instance.parameters().first_stage_orientation is Vertical or Horizontal,
 * at most one solution is returned, respecting that orientation (even when the
 * item list is empty).
 *
 * If instance.parameters().first_stage_orientation is Any, up to two solutions
 * are returned — one for each orientation that yields a non-empty first stage.
 */
std::vector<Solution> build_guillotine_solutions(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemPlacement>& item_placements);

/**
 * Like build_guillotine_solutions, but always returns at most one Solution.
 * When both orientations are valid, returns the one with the fewest stages.
 * Returns an empty Solution (number_of_bins() == 0) if no guillotine cut
 * is possible.
 */
Solution build_guillotine_solution(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemPlacement>& item_placements);

/**
 * Recompute a solution with a minimum number of stages.
 *
 * For each bin, item placements are extracted from the existing nodes and
 * passed to build_guillotine_solution, which reconstructs the guillotine cut
 * tree with the fewest stages possible for those item positions.  If
 * rebuilding succeeds, the new tree replaces the original bin; otherwise the
 * original bin is kept unchanged.
 */
Solution minimize_number_of_stages(const Solution& solution);

}
}
