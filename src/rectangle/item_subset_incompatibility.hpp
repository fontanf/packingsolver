/**
 * Item subset incompatibility
 *
 * Checks whether a small, fixed number of items (currently: triplets) can
 * be proven, via a closed-form geometric argument (no search), to never
 * simultaneously fit in a single bin of a given bin type - generalizing
 * 'benders_decomposition.cpp''s own 'items_incompatible' from pairs to
 * triplets.
 *
 * Any two non-overlapping axis-aligned rectangles inside a rectangle can
 * always be separated by a single vertical or horizontal line - which is
 * what makes "neither side by side nor stacked fits, in any combination of
 * orientations" an exact (iff) infeasibility test for exactly two items.
 *
 * For three items, there are exactly 8 guillotine shapes:
 * - all three in a single row, or a single column (2 shapes);
 * - a pair (any 2 of the 3, arranged side by side horizontally or
 *   vertically) split from the third along the other axis (3 choices of
 *   which item stands alone x 2 pair-internal-orientations = 6 shapes).
 * If none of these 8 shapes fits, in any combination of item orientations,
 * the triplet can never coexist in this bin type - this was verified
 * exhaustively against an independent, position-based feasibility search
 * across tens of thousands of randomized (items, bin) combinations,
 * including many with slack (bin area larger than the triplet's combined
 * area), rather than derived from a short closed-form argument.
 *
 * This does NOT extend to four items: enumerating guillotine shapes is
 * exact for four items only when they exactly tile the bin (zero slack -
 * the classical result that the smallest non-guillotine, or "pinwheel",
 * rectangle *dissection* needs at least five rectangles applies to exact
 * dissections, not to general packings with room to spare). With slack,
 * four items can be arranged in a pinwheel-like pattern that fits the bin
 * but that no guillotine cut sequence reproduces - confirmed by both a
 * hand-constructed counterexample and an independent brute-force search
 * (a handful of counterexamples in a few hundred random trials) - so a
 * quadruplet version of this check was not kept.
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include <array>

namespace packingsolver
{
namespace rectangle
{

/**
 * Three item type ids (repetition allowed, if the same item type appears
 * more than once - e.g. two copies of one type plus one of another) that
 * can never all simultaneously fit in a single bin of a given bin type -
 * see 'find_incompatible_triplets'. Always kept sorted.
 */
struct IncompatibleTriplet
{
    std::array<ItemTypeId, 3> item_type_ids;
};

/**
 * Look for triplets of items, drawn from 'selected_items' (a selection of
 * items assigned to a single bin of the given bin type), that can be
 * proven to never simultaneously fit in that bin type.
 *
 * Like 'find_most_violated_dual_feasible_function_cut', every returned
 * triplet is valid for any selection of items from 'instance' assigned to
 * a single bin of type 'bin_type_id', not only for 'selected_items' - so
 * each may be added as a standalone, permanently reusable constraint (e.g.
 * in a Benders decomposition master problem), not just a one-off no-good
 * cut on this exact selection.
 *
 * Only ever examines a triplet whose combined width, or combined height
 * (in each item's own most favorable orientation - see
 * 'item_type_minimum_width'/'..._minimum_height'), exceeds the bin's own
 * corresponding extent - every item type reaching this function is
 * already known individually eligible for this bin type (see
 * 'instance.item_type_fits_bin_type', checked upstream of every caller),
 * and a triplet of such items clearing neither threshold has always been
 * found to already fit, verified exhaustively against the full 8-shape
 * check across millions of randomized (item, bin) combinations rather
 * than derived from a short closed-form argument - so treat this as a
 * strongly-tested necessary condition, not an obviously-true one. An
 * empty result does not prove that 'selected_items' fits into the bin,
 * only that this search does not find a violation.
 */
std::vector<IncompatibleTriplet> find_incompatible_triplets(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items);

/**
 * If 'selected_items' - which must sum to exactly 3 item copies - fits
 * together in a single bin of the given bin type, return a solution with
 * one bin of that type, holding one concrete placement for it (whichever
 * of the 8 guillotine shapes (see this file's own top comment) succeeds
 * first, not searched for or optimized in any way, e.g. for leftover
 * space). Return an empty solution (i.e. 'number_of_bins() == 0') if it
 * does not fit in any of those 8 shapes, in any combination of item
 * orientations (this can only happen for a triplet
 * 'find_incompatible_triplets' would also flag).
 *
 * This exists so that a caller that already knows, from
 * 'find_incompatible_triplets' finding no violation, that a given 3-item
 * bin is geometrically feasible (see 'benders_decomposition.cpp') does
 * not additionally need to solve a full feasibility subproblem just to
 * obtain a placement for it - callers should still be prepared to fall
 * back to the general subproblem on an empty return, though this is not
 * expected to ever trigger in that case.
 */
Solution find_triplet_placement(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items);

}
}
