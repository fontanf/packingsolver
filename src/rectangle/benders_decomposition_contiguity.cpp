#include "rectangle/benders_decomposition_contiguity.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/solution_builder.hpp"
#include "rectangle/onedimentional_contiguity/milp.hpp"
#include "rectangle/onedimentional_contiguity/tree_search.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;
using namespace packingsolver::rectangle::onedimentional_contiguity;

namespace
{

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// y-check //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * A unit with an already-fixed x-position (as chosen by the BMP), input to
 * 'y_check'.
 */
struct YCheckUnit
{
    Length x;
    Length width;
    Length height;
};

/** Outcome of 'y_check' below. */
enum class YCheckStatus
{
    /** A feasible y-assignment was found. */
    Feasible,
    /** No y-assignment exists at all. */
    Infeasible,
    /** 'timer' ended before either conclusion could be reached. */
    Inconclusive,
};

/** Result of 'y_check' below. */
struct YCheckResult
{
    YCheckStatus status = YCheckStatus::Inconclusive;

    /** Only meaningful if 'status == Feasible'; indexed like the 'units' passed to 'y_check'. */
    std::vector<Length> y;
};

namespace ycheck
{

/**
 * One item as seen by the branch-and-bound core and the preprocessing
 * routines below: either a single original unit, or - once
 * 'preprocess_merge' has fused some of them together - several original
 * units glued into a single atomic block, exactly as wide/tall as their
 * combined footprint. 'member_unit_ids'/'member_rel_y' let the final
 * y-assignment be unfused again once the block's own position is decided by
 * the search: original unit 'member_unit_ids[k]' ends up at
 * 'block_y + member_rel_y[k]', where 'block_y' is this block's own position
 * as found by 'search'.
 *
 * 'x'/'width' are the block's *search* geometry: 'preprocess_lift_widths'
 * below may widen them past the block's own members' real footprint (sound
 * because, by construction, no other block can ever occupy the widened
 * slack - see its own doc comment); the real, per-member x is untouched
 * throughout (it lives in the caller's own 'YCheckUnit's, never copied into
 * 'Block') since only 'y' is ever reported back.
 */
struct Block
{
    Length x;
    Length width;
    Length height;

    std::vector<size_t> member_unit_ids;
    std::vector<Length> member_rel_y;
};

/** Outcome of one 'search' call - see its own doc comment. */
enum class NodeStatus
{
    Feasible,
    Infeasible,
    TimedOut,
};

/**
 * The niche/skyline branch-and-bound of Côté, Dell'Amico & Iori (2014,
 * §3.2, "Enumeration Tree for Problem y-check") / Wang et al. (2025,
 * Appendix G.2), including all five of their fathoming rules and the
 * gap-closing skyline update that kills the symmetry between equivalent
 * orderings of the same final layout - y-check is strongly NP-complete
 * (Côté et al. 2014, Theorem 1), and none of this implements around that;
 * it only implements the pruning the reference algorithm relies on to stay
 * practical, plus (via 'timer') the same bounded-search discipline used
 * everywhere else in this file.
 *
 * One call per enumeration-tree node; 'h_used' is the current skyline
 * (mutated in place and restored to its pre-call state before returning on
 * every non-'Feasible' path). 'placed'/'block_y' are indexed like 'blocks';
 * on 'Feasible', every entry of 'placed' is 'true' and 'block_y' holds each
 * block's own final y.
 *
 * Recursive rather than the iterative/explicit-stack style used elsewhere
 * in this file: recursion depth is bounded by 'blocks.size()' (one call per
 * successfully-placed block, plus a bounded number of "close the niche"
 * calls in between), which stays small even for the largest instances this
 * algorithm targets, and the fathoming-rule bookkeeping below reads far
 * more directly this way.
 */
NodeStatus search(
        Length bin_width,
        Length bin_height,
        const std::vector<Block>& blocks,
        const optimizationtools::Timer& timer,
        std::vector<Length>& h_used,
        std::vector<bool>& placed,
        std::vector<Length>& block_y,
        ItemPos number_of_unplaced)
{
    if (timer.needs_to_end())
        return NodeStatus::TimedOut;

    if (number_of_unplaced == 0)
        return NodeStatus::Feasible;

    // Niche: leftmost maximal run of columns at the skyline's global
    // minimum height.
    Length min_height = h_used[0];
    for (Length c = 1; c < bin_width; ++c)
        min_height = std::min(min_height, h_used[c]);
    Length niche_left = 0;
    while (h_used[niche_left] != min_height)
        ++niche_left;
    Length niche_right = niche_left;
    while (niche_right + 1 < bin_width && h_used[niche_right + 1] == min_height)
        ++niche_right;

    Length h_left = (niche_left == 0)? bin_height: h_used[niche_left - 1];
    Length h_right = (niche_right == bin_width - 1)? bin_height: h_used[niche_right + 1];

    // Fathoming 1: even packing every still-unplaced block on top of the
    // current skyline (its own best case) already overflows some column.
    {
        std::vector<Length> h_pack(bin_width, 0);
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (placed[i])
                continue;
            const Block& block = blocks[i];
            for (Length c = block.x; c < block.x + block.width; ++c)
                h_pack[c] += block.height;
        }
        for (Length c = 0; c < bin_width; ++c) {
            if (h_used[c] + h_pack[c] > bin_height)
                return NodeStatus::Infeasible;
        }
    }

    // Candidate blocks: unplaced, entirely contained in the niche, and
    // whose height fits below 'bin_height'.
    std::vector<size_t> candidates;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (placed[i])
            continue;
        const Block& block = blocks[i];
        if (block.x < niche_left || block.x + block.width - 1 > niche_right)
            continue;
        if (min_height + block.height > bin_height)
            continue;
        candidates.push_back(i);
    }
    // Branching order (also required by fathoming rules 3-5 below, which
    // compare candidates pairwise in this order): nondecreasing x.
    std::sort(candidates.begin(), candidates.end(),
            [&blocks](size_t a, size_t b) { return blocks[a].x < blocks[b].x; });

    // Fathoming 2: a candidate spanning the *entire* niche (leaving no
    // leftover gap for the gap-closing update above to act on) whose own
    // height doesn't overshoot either neighboring wall dominates every
    // other branch, including "close the niche without packing anything" -
    // it settles the niche in one step at least as well as any combination
    // of narrower candidates could, since none of those could do so without
    // itself leaving a gap here. Requiring the full span is essential: a
    // narrower candidate meeting only the height condition is not
    // dominant - a different, still-unplaced candidate for the leftover
    // part of the niche can depend on that part staying at 'min_height'
    // (e.g. two same-column blocks that must stack to exactly fill
    // 'bin_height' together), and this candidate closing that leftover
    // part's gap up to its own (lower) height first can push the required
    // stack height past 'bin_height', wrongly declaring infeasible what
    // choosing another candidate first would have solved.
    bool dominant_found = false;
    for (size_t i: candidates) {
        const Block& block = blocks[i];
        if (block.x == niche_left
                && block.x + block.width - 1 == niche_right
                && min_height + block.height <= std::min(h_left, h_right)) {
            candidates.assign(1, i);
            dominant_found = true;
            break;
        }
    }

    for (size_t i: candidates) {
        const Block& block = blocks[i];

        // Fathoming 3: twin blocks (same width/height/x) - only ever try
        // packing the lower-indexed one first.
        bool fathomed = false;
        for (size_t j = 0; j < blocks.size(); ++j) {
            if (j == i || placed[j])
                continue;
            const Block& other = blocks[j];
            if (j < i
                    && other.width == block.width
                    && other.height == block.height
                    && other.x == block.x) {
                fathomed = true;
                break;
            }
        }
        // Fathoming 4: a higher-indexed twin (same width/height/x) already
        // sits flush against the top of the skyline at this same column -
        // the height must match too, otherwise this is not actually the
        // same block placed in a different order, just an unrelated block
        // that happens to occupy the same column and width.
        if (!fathomed) {
            for (size_t k = 0; k < blocks.size(); ++k) {
                if (!placed[k])
                    continue;
                const Block& other = blocks[k];
                if (k > i
                        && other.width == block.width
                        && other.height == block.height
                        && other.x == block.x
                        && block_y[k] + other.height == min_height) {
                    fathomed = true;
                    break;
                }
            }
        }
        // Fathoming 5: some other still-unplaced block fits entirely
        // inside the gap that packing 'block' (at 'x > niche_left') would
        // close off.
        if (!fathomed && block.x > niche_left) {
            for (size_t j = 0; j < blocks.size(); ++j) {
                if (j == i || placed[j])
                    continue;
                const Block& other = blocks[j];
                if (other.x >= niche_left
                        && other.x + other.width <= block.x
                        && other.height <= std::min(h_left - min_height, block.height)) {
                    fathomed = true;
                    break;
                }
            }
        }
        if (fathomed)
            continue;

        // Place 'block': close the gap between the niche's left border and
        // 'block.x' (Wang et al. 2025, eq. (25)) - kills the symmetry
        // between every ordering of the units that could otherwise fill
        // that gap, since none of them can ever end up forced to a smaller
        // x than 'block.x' within this niche - then stack 'block' itself.
        for (Length c = niche_left; c < block.x; ++c)
            h_used[c] = std::min(h_left, min_height + block.height);
        for (Length c = block.x; c < block.x + block.width; ++c)
            h_used[c] = min_height + block.height;
        placed[i] = true;
        block_y[i] = min_height;

        NodeStatus child_status = search(
                bin_width, bin_height, blocks, timer,
                h_used, placed, block_y, number_of_unplaced - 1);
        if (child_status != NodeStatus::Infeasible)
            return child_status;

        placed[i] = false;
        for (Length c = niche_left; c <= niche_right; ++c)
            h_used[c] = min_height;
    }

    // "Close the niche" without packing anything - skipped entirely once
    // fathoming 2 already found a dominant candidate above.
    if (!dominant_found) {
        Length closed_height = std::min(h_left, h_right);
        // Always a strict increase (the niche is, by construction, the
        // *leftmost* run at the *global* minimum height, so any neighbor
        // is either off the bin entirely - the 'bin_height' stand-in - or
        // strictly taller), except in the fully-closed case where every
        // column already sits at 'bin_height' - guarded against here to
        // guarantee termination.
        if (closed_height > min_height && closed_height <= bin_height) {
            for (Length c = niche_left; c <= niche_right; ++c)
                h_used[c] = closed_height;

            NodeStatus child_status = search(
                    bin_width, bin_height, blocks, timer,
                    h_used, placed, block_y, number_of_unplaced);
            if (child_status != NodeStatus::Infeasible)
                return child_status;

            for (Length c = niche_left; c <= niche_right; ++c)
                h_used[c] = min_height;
        }
    }

    return NodeStatus::Infeasible;
}

/**
 * Run 'search' from an empty skyline on 'blocks', returning the found
 * 'block_y' (only meaningful on 'NodeStatus::Feasible').
 */
NodeStatus search_from_scratch(
        Length bin_width,
        Length bin_height,
        const std::vector<Block>& blocks,
        const optimizationtools::Timer& timer,
        std::vector<Length>& block_y)
{
    std::vector<Length> h_used(bin_width, 0);
    std::vector<bool> placed(blocks.size(), false);
    block_y.assign(blocks.size(), 0);
    return search(
            bin_width, bin_height, blocks, timer,
            h_used, placed, block_y, (ItemPos)blocks.size());
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Preprocessing //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Preprocessing 2 (Côté et al. 2014, §3.2, "Lift Item Widths"): widen every
 * block's own search footprint as far as the free horizontal band around it
 * allows - sound because, by construction of that band (bounded by the
 * nearest block that can never sit beside it), no other block can ever
 * occupy the extra slack, so treating it as part of this block's own
 * footprint for skyline bookkeeping cannot change y-feasibility. Mutates
 * 'blocks' in place, processing them by nondecreasing width, then height,
 * so a block already widened earlier in the pass is used as such (via its
 * updated 'x'/'width') by later ones.
 */
void preprocess_lift_widths(
        Length bin_width,
        std::vector<Block>& blocks)
{
    std::vector<size_t> order(blocks.size());
    for (size_t i = 0; i < blocks.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(),
            [&blocks](size_t a, size_t b)
            {
                if (blocks[a].width != blocks[b].width)
                    return blocks[a].width < blocks[b].width;
                return blocks[a].height < blocks[b].height;
            });

    for (size_t i: order) {
        const Block& block = blocks[i];
        Length l = 0;
        Length r = bin_width;
        for (size_t j = 0; j < blocks.size(); ++j) {
            if (j == i)
                continue;
            const Block& other = blocks[j];
            if (other.x + other.width <= block.x)
                l = std::max(l, other.x + other.width);
            if (other.x >= block.x + block.width)
                r = std::min(r, other.x);
        }
        blocks[i].x = l;
        blocks[i].width = r - l;
    }
}

/**
 * Preprocessing 3 (Côté et al. 2014, §3.2, "Shrink the Strip"): returns a
 * copy of 'blocks' with every 'x'/'width' remapped onto only the columns
 * where some block's own left edge lies - since every block's own 'x' is
 * one of those columns by construction, no block's left edge ever falls
 * strictly between two of them, so the packing of any set of blocks is
 * feasible at one such column iff it is feasible at every column up to the
 * next one (nothing but a block's own *right* edge - never load-bearing,
 * since fewer covering blocks only ever makes a column easier - can fall
 * strictly in between). Hence 'search_from_scratch' on the result reports
 * exactly the same 'block_y' (indexed the same way, one per input block) as
 * it would have on the original 'blocks'. '*compact_bin_width' is set to
 * the resulting (possibly much smaller) strip width.
 */
std::vector<Block> preprocess_shrink_strip(
        const std::vector<Block>& blocks,
        Length* compact_bin_width)
{
    std::vector<Length> breakpoints;
    breakpoints.push_back(0);
    for (const Block& block: blocks)
        breakpoints.push_back(block.x);
    std::sort(breakpoints.begin(), breakpoints.end());
    breakpoints.erase(
            std::unique(breakpoints.begin(), breakpoints.end()),
            breakpoints.end());

    auto compact_index = [&breakpoints](Length real_x) -> Length
    {
        // Index of the largest breakpoint <= 'real_x'.
        auto it = std::upper_bound(breakpoints.begin(), breakpoints.end(), real_x);
        return (Length)(std::distance(breakpoints.begin(), it) - 1);
    };

    std::vector<Block> result = blocks;
    for (Block& block: result) {
        Length compact_x = compact_index(block.x);
        Length compact_right = compact_index(block.x + block.width - 1);
        block.x = compact_x;
        block.width = compact_right - compact_x + 1;
    }
    *compact_bin_width = (Length)breakpoints.size();
    return result;
}

/**
 * Preprocessing 1 (Côté et al. 2014, §3.2, "Merge Items"), one direction
 * (see 'preprocess_merge' below for the two symmetric passes over 'blocks'
 * that call this): attempt to fuse 'blocks[pivot_index]' with some
 * (possibly reduced, per Côté et al.'s own "second step") subset of the
 * blocks entirely to its own left, i.e. those with 'x + width <= pivot.x' -
 * only ever committed once verified, via a real (nested) recursive
 * 'search_from_scratch' call, to actually fit within a substrip of the
 * candidate subset's own width and 'pivot.height'. Returns 'true' (and
 * mutates 'blocks': 'pivot_index' and every merged block replaced by the
 * single fused block, appended at the end) iff a merge was found; leaves
 * 'blocks' untouched otherwise. Sets '*timed_out' if a nested search ran
 * out of time - preprocessing is purely an optional strength reduction, so
 * this is never a correctness concern, only a reason for the caller to
 * stop attempting any further reduction.
 */
bool try_merge_left(
        std::vector<Block>& blocks,
        size_t pivot_index,
        const optimizationtools::Timer& timer,
        bool* timed_out)
{
    const Block& pivot = blocks[pivot_index];
    if (pivot.x == 0)
        return false;

    std::vector<size_t> left;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (i == pivot_index)
            continue;
        if (blocks[i].x + blocks[i].width <= pivot.x)
            left.push_back(i);
    }
    if (left.empty())
        return false;

    // Candidate boundaries, from Côté et al.'s own "second step": every
    // distinct 'x + width' among 'left', tried in increasing order, plus 0
    // (the first attempt, corresponding to their "first step", is thus
    // always the full, unreduced 'left').
    std::vector<Length> boundaries;
    boundaries.push_back(0);
    for (size_t i: left)
        boundaries.push_back(blocks[i].x + blocks[i].width);
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    for (Length boundary: boundaries) {
        // The reduced subset: every 'left' block entirely at or past
        // 'boundary' - only a genuine partition (no block straddles it) is
        // ever attempted, so the reduced instance stays self-contained.
        bool straddles = false;
        std::vector<size_t> reduced;
        for (size_t i: left) {
            if (blocks[i].x < boundary && blocks[i].x + blocks[i].width > boundary) {
                straddles = true;
                break;
            }
            if (blocks[i].x >= boundary)
                reduced.push_back(i);
        }
        if (straddles || reduced.empty())
            continue;

        bool heights_fit = true;
        for (size_t i: reduced) {
            if (blocks[i].height > pivot.height) {
                heights_fit = false;
                break;
            }
        }
        if (!heights_fit)
            continue;

        std::vector<Block> sub_blocks;
        for (size_t i: reduced) {
            Block sub_block = blocks[i];
            sub_block.x -= boundary;
            sub_blocks.push_back(sub_block);
        }
        std::vector<Length> sub_y;
        NodeStatus status = search_from_scratch(
                pivot.x - boundary, pivot.height, sub_blocks, timer, sub_y);
        if (status == NodeStatus::TimedOut) {
            *timed_out = true;
            return false;
        }
        if (status != NodeStatus::Feasible)
            continue;

        // Commit the merge: fuse 'pivot' and 'reduced' into one block
        // spanning ['boundary', pivot.x + pivot.width), height
        // 'pivot.height'.
        Block merged;
        merged.x = boundary;
        merged.width = pivot.x + pivot.width - boundary;
        merged.height = pivot.height;
        for (size_t k = 0; k < reduced.size(); ++k) {
            const Block& sub_block = sub_blocks[k];
            for (size_t m = 0; m < sub_block.member_unit_ids.size(); ++m) {
                merged.member_unit_ids.push_back(sub_block.member_unit_ids[m]);
                merged.member_rel_y.push_back(sub_y[k] + sub_block.member_rel_y[m]);
            }
        }
        for (size_t m = 0; m < pivot.member_unit_ids.size(); ++m) {
            merged.member_unit_ids.push_back(pivot.member_unit_ids[m]);
            merged.member_rel_y.push_back(pivot.member_rel_y[m]);
        }

        std::vector<size_t> to_remove = reduced;
        to_remove.push_back(pivot_index);
        std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
        for (size_t i: to_remove)
            blocks.erase(blocks.begin() + i);
        blocks.push_back(merged);
        return true;
    }
    return false;
}

/** Mirror of 'try_merge_left' above, fusing 'blocks[pivot_index]' with a subset of the blocks entirely to its own right. */
bool try_merge_right(
        std::vector<Block>& blocks,
        size_t pivot_index,
        Length bin_width,
        const optimizationtools::Timer& timer,
        bool* timed_out)
{
    const Block& pivot = blocks[pivot_index];
    if (pivot.x + pivot.width == bin_width)
        return false;

    std::vector<size_t> right;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (i == pivot_index)
            continue;
        if (blocks[i].x >= pivot.x + pivot.width)
            right.push_back(i);
    }
    if (right.empty())
        return false;

    std::vector<Length> boundaries;
    boundaries.push_back(bin_width);
    for (size_t i: right)
        boundaries.push_back(blocks[i].x);
    std::sort(boundaries.begin(), boundaries.end(), std::greater<Length>());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    for (Length boundary: boundaries) {
        bool straddles = false;
        std::vector<size_t> reduced;
        for (size_t i: right) {
            if (blocks[i].x < boundary && blocks[i].x + blocks[i].width > boundary) {
                straddles = true;
                break;
            }
            if (blocks[i].x + blocks[i].width <= boundary)
                reduced.push_back(i);
        }
        if (straddles || reduced.empty())
            continue;

        bool heights_fit = true;
        for (size_t i: reduced) {
            if (blocks[i].height > pivot.height) {
                heights_fit = false;
                break;
            }
        }
        if (!heights_fit)
            continue;

        std::vector<Block> sub_blocks;
        for (size_t i: reduced) {
            Block sub_block = blocks[i];
            sub_block.x -= (pivot.x + pivot.width);
            sub_blocks.push_back(sub_block);
        }
        std::vector<Length> sub_y;
        NodeStatus status = search_from_scratch(
                boundary - (pivot.x + pivot.width), pivot.height, sub_blocks, timer, sub_y);
        if (status == NodeStatus::TimedOut) {
            *timed_out = true;
            return false;
        }
        if (status != NodeStatus::Feasible)
            continue;

        Block merged;
        merged.x = pivot.x;
        merged.width = boundary - pivot.x;
        merged.height = pivot.height;
        for (size_t m = 0; m < pivot.member_unit_ids.size(); ++m) {
            merged.member_unit_ids.push_back(pivot.member_unit_ids[m]);
            merged.member_rel_y.push_back(pivot.member_rel_y[m]);
        }
        for (size_t k = 0; k < reduced.size(); ++k) {
            const Block& sub_block = sub_blocks[k];
            for (size_t m = 0; m < sub_block.member_unit_ids.size(); ++m) {
                merged.member_unit_ids.push_back(sub_block.member_unit_ids[m]);
                merged.member_rel_y.push_back(sub_y[k] + sub_block.member_rel_y[m]);
            }
        }

        std::vector<size_t> to_remove = reduced;
        to_remove.push_back(pivot_index);
        std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
        for (size_t i: to_remove)
            blocks.erase(blocks.begin() + i);
        blocks.push_back(merged);
        return true;
    }
    return false;
}

/**
 * Preprocessing 1 (Côté et al. 2014, §3.2, "Merge Items"): two passes over
 * 'blocks' - left merges (nonincreasing x), then right merges (nondecreasing
 * x) - each repeated (restarting the scan after every successful merge,
 * since block indices shift and a newly-fused block may itself merge
 * further) until a full scan finds nothing left to fuse. Mutates 'blocks'
 * in place; sets '*timed_out' (and returns early) the moment any nested
 * verification search runs out of time.
 */
void preprocess_merge(
        Length bin_width,
        std::vector<Block>& blocks,
        const optimizationtools::Timer& timer,
        bool* timed_out)
{
    bool changed = true;
    while (changed) {
        changed = false;
        if (timer.needs_to_end()) {
            *timed_out = true;
            return;
        }
        std::vector<size_t> order(blocks.size());
        for (size_t i = 0; i < blocks.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(),
                [&blocks](size_t a, size_t b) { return blocks[a].x > blocks[b].x; });
        for (size_t i: order) {
            if (try_merge_left(blocks, i, timer, timed_out)) {
                changed = true;
                break;
            }
            if (*timed_out)
                return;
        }
    }

    changed = true;
    while (changed) {
        changed = false;
        if (timer.needs_to_end()) {
            *timed_out = true;
            return;
        }
        std::vector<size_t> order(blocks.size());
        for (size_t i = 0; i < blocks.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(),
                [&blocks](size_t a, size_t b) { return blocks[a].x < blocks[b].x; });
        for (size_t i: order) {
            if (try_merge_right(blocks, i, bin_width, timer, timed_out)) {
                changed = true;
                break;
            }
            if (*timed_out)
                return;
        }
    }
}

}

/**
 * The 'y-check' slave problem (BSP): given each selected unit's fixed
 * x-position (and, implicitly, its orientation, via 'width'/'height'), find
 * y-positions for every unit such that no two overlap, if any exist.
 *
 * y-check is strongly NP-complete (Côté, Dell'Amico & Iori 2014, Theorem
 * 1), so - unlike a typical polynomial subproblem - this is bounded by
 * 'timer' rather than guaranteed to terminate quickly:
 * 'YCheckStatus::Inconclusive' means 'timer' ended before either a feasible
 * y-assignment or a proof that none exists was found. Implements the full
 * combinatorial branch-and-bound of Côté, Dell'Amico & Iori (2014, §3.2) /
 * Wang et al. (2025, Appendix G.2): the three preprocessing reductions
 * (merge items, lift item widths, shrink the strip - 'ycheck::
 * preprocess_merge'/'preprocess_lift_widths'/'preprocess_shrink_strip'
 * above), then the gap-closing, five-fathoming-rule enumeration tree
 * ('ycheck::search' above).
 */
YCheckResult y_check(
        Length bin_width,
        Length bin_height,
        const std::vector<YCheckUnit>& units,
        const optimizationtools::Timer& timer)
{
    YCheckResult result;
    if (units.empty()) {
        result.status = YCheckStatus::Feasible;
        return result;
    }

    std::vector<ycheck::Block> blocks(units.size());
    for (size_t i = 0; i < units.size(); ++i) {
        blocks[i].x = units[i].x;
        blocks[i].width = units[i].width;
        blocks[i].height = units[i].height;
        blocks[i].member_unit_ids.push_back(i);
        blocks[i].member_rel_y.push_back(0);
    }

    bool timed_out = false;
    ycheck::preprocess_merge(bin_width, blocks, timer, &timed_out);
    if (!timed_out)
        ycheck::preprocess_lift_widths(bin_width, blocks);

    Length compact_bin_width = bin_width;
    std::vector<ycheck::Block> search_blocks = blocks;
    if (!timed_out)
        search_blocks = ycheck::preprocess_shrink_strip(blocks, &compact_bin_width);

    std::vector<Length> block_y;
    ycheck::NodeStatus status = ycheck::search_from_scratch(
            compact_bin_width, bin_height, search_blocks, timer, block_y);

    if (status == ycheck::NodeStatus::TimedOut) {
        result.status = YCheckStatus::Inconclusive;
        return result;
    }
    if (status == ycheck::NodeStatus::Infeasible) {
        result.status = YCheckStatus::Infeasible;
        return result;
    }

    result.status = YCheckStatus::Feasible;
    result.y.assign(units.size(), 0);
    for (size_t i = 0; i < blocks.size(); ++i) {
        const ycheck::Block& block = blocks[i];
        for (size_t m = 0; m < block.member_unit_ids.size(); ++m)
            result.y[block.member_unit_ids[m]] = block_y[i] + block.member_rel_y[m];
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Cut strengthening /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * 'true' iff 'units' (their already-fixed x-positions, widths and heights)
 * are *proven* to admit no feasible y-assignment - the same BSP oracle
 * 'y_check' provides elsewhere in this file, just named for readability at
 * every call site below, folding 'YCheckStatus::Inconclusive' (timed out)
 * into 'false': every caller below only ever uses this to decide whether a
 * *known*-infeasible set can be shrunk further while staying infeasible, so
 * an unproven "maybe" must be treated the same as "no, don't shrink" rather
 * than risk keeping a reduction that was never actually verified. An empty
 * set is trivially feasible.
 */
bool infeasible(
        Length bin_width,
        Length bin_height,
        const std::vector<Candidate>& units,
        const optimizationtools::Timer& timer)
{
    if (units.empty())
        return false;
    std::vector<YCheckUnit> y_check_units;
    for (const Candidate& unit: units)
        y_check_units.push_back({unit.x, unit.width, unit.height});
    return y_check(bin_width, bin_height, y_check_units, timer).status == YCheckStatus::Infeasible;
}

/**
 * No-good cut strengthening, step (1) (Wang et al. 2025 §3.5.2): given
 * 'units' (already known 'infeasible'), recursively split it by a vertical
 * line at each of its own units' own x-position, into the units strictly to
 * its left and the units at or past it - a unit whose own interval
 * straddles the line belongs to neither side, and is simply dropped from
 * that particular split. Whichever side is both a strict subset and still
 * infeasible is recursed into; stops (returning 'units' unchanged) once no
 * split line yields any further reduction.
 */
std::vector<Candidate> shrink_via_split(
        Length bin_width,
        Length bin_height,
        const std::vector<Candidate>& units,
        const optimizationtools::Timer& timer)
{
    for (const Candidate& split_unit: units) {
        Length split_x = split_unit.x;
        std::vector<Candidate> left;
        std::vector<Candidate> right;
        for (const Candidate& unit: units) {
            if (unit.x + unit.width <= split_x)
                left.push_back(unit);
            if (unit.x >= split_x)
                right.push_back(unit);
        }
        if (!left.empty()
                && left.size() < units.size()
                && infeasible(bin_width, bin_height, left, timer)) {
            return shrink_via_split(bin_width, bin_height, left, timer);
        }
        if (!right.empty()
                && right.size() < units.size()
                && infeasible(bin_width, bin_height, right, timer)) {
            return shrink_via_split(bin_width, bin_height, right, timer);
        }
    }
    return units;
}

/**
 * No-good cut strengthening, step (2) (Wang et al. 2025 §3.5.2): scan
 * vertical lines left to right, dropping every unit whose own left edge
 * sits exactly on the current line; then right to left, dropping every
 * unit whose own interval contains the current line. After each drop, the
 * reduction is kept only if the remaining set stays infeasible; it is
 * restored (the scan simply continues with the unreduced set) otherwise.
 */
std::vector<Candidate> shrink_via_sweep(
        Length bin_width,
        Length bin_height,
        std::vector<Candidate> subset,
        const optimizationtools::Timer& timer)
{
    // Left to right: drop units whose own left edge is the current line.
    for (Length x = 0; x < bin_width; ++x) {
        std::vector<Candidate> trial;
        bool any_removed = false;
        for (const Candidate& unit: subset) {
            if (unit.x == x) {
                any_removed = true;
            } else {
                trial.push_back(unit);
            }
        }
        if (any_removed
                && !trial.empty()
                && infeasible(bin_width, bin_height, trial, timer)) {
            subset = std::move(trial);
        }
    }
    // Right to left: drop units whose own interval contains the current line.
    for (Length x = bin_width - 1; x >= 0; --x) {
        std::vector<Candidate> trial;
        bool any_removed = false;
        for (const Candidate& unit: subset) {
            if (unit.x <= x && x < unit.x + unit.width) {
                any_removed = true;
            } else {
                trial.push_back(unit);
            }
        }
        if (any_removed
                && !trial.empty()
                && infeasible(bin_width, bin_height, trial, timer)) {
            subset = std::move(trial);
        }
    }
    return subset;
}

/**
 * No-good cut strengthening, step (3) (Wang et al. 2025 §3.5.2): 12 scans
 * over the current subset - 6 deterministic orderings (non-decreasing
 * area, width, height, perimeter, x-coordinate, and "intersection score",
 * the number of other units in the current subset whose interval overlaps
 * this one's) plus 6 random orderings - each iterating in that order and
 * permanently removing a unit if the remaining set stays infeasible
 * without it (left alone, i.e. never actually removed from 'subset',
 * otherwise). Scans are chained sequentially: each one computes its own
 * order fresh from whatever 'subset' the previous scan left behind, so a
 * later scan can still shrink what an earlier one, in a different order,
 * could not.
 *
 * Units are compared for identity by value (see 'Candidate::operator==')
 * rather than by index into a shared array: since each unit's own
 * '(item_type_id, copy)' pair is unique within any subset built from a
 * single BMP solution, this is just as unambiguous as an index would be.
 */
std::vector<Candidate> shrink_via_removal(
        Length bin_width,
        Length bin_height,
        const Instance& instance,
        std::vector<Candidate> subset,
        std::mt19937_64& generator,
        const optimizationtools::Timer& timer)
{
    auto intersection_score = [&](const Candidate& unit)
    {
        int score = 0;
        for (const Candidate& other: subset) {
            if (other == unit)
                continue;
            if (unit.x < other.x + other.width
                    && other.x < unit.x + unit.width) {
                ++score;
            }
        }
        return score;
    };

    auto scan = [&](const std::vector<Candidate>& order)
    {
        for (const Candidate& unit: order) {
            if (std::find(subset.begin(), subset.end(), unit) == subset.end())
                continue;
            std::vector<Candidate> trial;
            for (const Candidate& other: subset)
                if (!(other == unit))
                    trial.push_back(other);
            if (!trial.empty()
                    && infeasible(bin_width, bin_height, trial, timer)) {
                subset = std::move(trial);
            }
        }
    };

    for (int order_type = 0; order_type < 6; ++order_type) {
        std::vector<Candidate> order = subset;
        switch (order_type) {
        case 0:
            std::sort(order.begin(), order.end(),
                    [&](const Candidate& a, const Candidate& b)
                    {
                        const ItemType& item_type_a = instance.item_type(a.item_type_id);
                        const ItemType& item_type_b = instance.item_type(b.item_type_id);
                        return item_type_a.rect.area() < item_type_b.rect.area();
                    });
            break;
        case 1:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b) { return a.width < b.width; });
            break;
        case 2:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b) { return a.height < b.height; });
            break;
        case 3:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b)
                    {
                        return 2 * (a.width + a.height) < 2 * (b.width + b.height);
                    });
            break;
        case 4:
            std::sort(order.begin(), order.end(),
                    [](const Candidate& a, const Candidate& b) { return a.x < b.x; });
            break;
        case 5:
            std::sort(order.begin(), order.end(),
                    [&](const Candidate& a, const Candidate& b)
                    {
                        return intersection_score(a) < intersection_score(b);
                    });
            break;
        }
        scan(order);
    }
    for (int i = 0; i < 6; ++i) {
        std::vector<Candidate> order = subset;
        std::shuffle(order.begin(), order.end(), generator);
        scan(order);
    }
    return subset;
}

/**
 * No-good cut strengthening, step (4) (Wang et al. 2025 §3.5.2, eqs.
 * (18a)-(18d)): given the final shrunk infeasible 'subset' (each unit
 * already at a fixed (x, orientation) from the BMP solution that failed
 * 'y_check'), solve an LP that widens each unit's own forbidden window
 * [l_i, r_i] as far as possible while guaranteeing every pair of units that
 * overlaps at its original fixed position keeps overlapping for any
 * positions within their own widened windows - i.e. the infeasibility
 * 'subset' witnesses is preserved throughout the whole product of windows,
 * not just at the single point actually solved. The final cut then covers
 * every one of a unit's own candidates (same orientation as originally
 * chosen - the LP's own overlap argument used that orientation's
 * width/height) whose x falls in [ceil(l_i*), floor(r_i*)], rather than
 * just the single one actually used - strictly stronger than
 * 'onedimentional_contiguity::build_positional_no_good_cut' while still
 * short of 'onedimentional_contiguity::build_covering_cut' (only sound once
 * every position has been ruled out, not merely a provably-still-infeasible
 * window).
 *
 * Rebuilds its own 'UnitsAndCandidates' from 'instance' (see
 * 'build_units_and_candidates') to look up each unit's own full candidate
 * list, needed for the final "every candidate within the lifted window"
 * step.
 */
NoGoodCut build_lifted_cut(
        Length bin_width,
        const Instance& instance,
        const std::vector<Candidate>& subset,
        const BendersDecompositionContiguityParameters& parameters)
{
    size_t n = subset.size();

    // overlaps[j] = indices (into 'subset') of units overlapping unit
    // 'subset[j]' at their original fixed positions.
    std::vector<std::vector<size_t>> overlaps(n);
    for (size_t j = 0; j < n; ++j) {
        const Candidate& candidate_j = subset[j];
        for (size_t i = 0; i < n; ++i) {
            if (i == j)
                continue;
            const Candidate& candidate_i = subset[i];
            if (candidate_j.x < candidate_i.x + candidate_i.width
                    && candidate_i.x < candidate_j.x + candidate_j.width) {
                overlaps[j].push_back(i);
            }
        }
    }

    // Variables: l_i at index 2 * i, r_i at index 2 * i + 1.
    mathoptsolverscmake::MathOptModel model((int)(2 * n), 0, 0);
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    for (size_t i = 0; i < n; ++i) {
        const Candidate& candidate_i = subset[i];
        model.variables_types[2 * i] = mathoptsolverscmake::VariableType::Continuous;
        model.variables_types[2 * i + 1] = mathoptsolverscmake::VariableType::Continuous;
        // (18c): 0 <= l_i <= x_i.
        model.variables_lower_bounds[2 * i] = 0.0;
        model.variables_upper_bounds[2 * i] = (double)candidate_i.x;
        // (18d): x_i <= r_i <= W - w_i.
        model.variables_lower_bounds[2 * i + 1] = (double)candidate_i.x;
        model.variables_upper_bounds[2 * i + 1] = (double)(bin_width - candidate_i.width);
        // (18a): max sum (r_i - l_i).
        model.objective_coefficients[2 * i] = -1.0;
        model.objective_coefficients[2 * i + 1] = 1.0;
    }
    // (18b): l_j + w_j >= r_i + 1  <=>  r_i - l_j <= w_j - 1, for every
    // ordered pair (j, i) with i overlapping j at their original
    // positions - enumerating every such ordered pair (not just unordered
    // ones) covers both directions of the inequality needed to guarantee
    // mutual overlap throughout the whole [l, r] product (see the
    // function's own doc comment).
    for (size_t j = 0; j < n; ++j) {
        const Candidate& candidate_j = subset[j];
        for (size_t i: overlaps[j]) {
            model.constraints_starts.push_back((int)model.elements_variables.size());
            model.elements_variables.push_back((int)(2 * i + 1));
            model.elements_coefficients.push_back(1.0);
            model.elements_variables.push_back((int)(2 * j));
            model.elements_coefficients.push_back(-1.0);
            model.constraints_lower_bounds.push_back(-std::numeric_limits<double>::infinity());
            model.constraints_upper_bounds.push_back((double)candidate_j.width - 1.0);
        }
    }

    if (parameters.master_problem_milp_solver != mathoptsolverscmake::SolverName::Highs)
        throw std::invalid_argument(FUNC_SIGNATURE);

    std::vector<double> solution;
#ifdef HIGHS_FOUND
    Highs highs;
    mathoptsolverscmake::reduce_printout(highs);
    mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
    mathoptsolverscmake::load(highs, model);
    mathoptsolverscmake::solve(highs);
    bool proven_infeasible =
        (highs.getModelStatus() == HighsModelStatus::kInfeasible
         || highs.getModelStatus() == HighsModelStatus::kUnboundedOrInfeasible);
    if (!proven_infeasible)
        solution = mathoptsolverscmake::get_solution(highs);
#else
    throw std::invalid_argument(FUNC_SIGNATURE);
#endif

    // The LP is always feasible ('l_i = r_i = x_i' for every i trivially
    // satisfies every (18b) row, since it just restates that the units
    // already overlap at their own original positions), so only a timeout
    // could leave 'solution' empty - fall back to the un-lifted window
    // ([x_i, x_i], still valid, just not widened) in that case.
    UnitsAndCandidates units_and_candidates = build_units_and_candidates(instance, instance.bin_type(0));
    NoGoodCut cut;
    cut.upper_bound = (ItemPos)n - 1;
    for (size_t i = 0; i < n; ++i) {
        const Candidate& candidate_i = subset[i];
        Length l = solution.empty()?
            candidate_i.x:
            (Length)std::ceil(solution[2 * i] - 1e-6);
        Length r = solution.empty()?
            candidate_i.x:
            (Length)std::floor(solution[2 * i + 1] + 1e-6);
        const std::vector<size_t>& unit_candidates
            = units_and_candidates.candidates_by_item_type_and_copy
                [candidate_i.item_type_id][candidate_i.copy];
        for (size_t candidate_id: unit_candidates) {
            const Candidate& other = units_and_candidates.candidates[candidate_id];
            if (other.rotate == candidate_i.rotate && other.x >= l && other.x <= r)
                cut.candidate_ids.push_back(candidate_id);
        }
    }
    return cut;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// LBD /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Result of 'run_lbd' below.
 */
struct LbdResult
{
    enum class Status
    {
        /** A feasible pattern for the whole item set was found (possibly with different x-positions than the outer BMP's own choice). */
        Feasible,
        /** No pattern exists for this item set at all, at any x-positions. */
        Infeasible,
        /** Timed out before reaching either conclusion. */
        Inconclusive,
    };

    /** Constructor. */
    LbdResult(const Instance& instance): solution(instance) { }

    Status status = Status::Inconclusive;

    /**
     * The pattern for the item set, in 'instance''s own bin/item type ids
     * (only meaningful if 'status == Feasible').
     */
    Solution solution;
};

/**
 * Local Benders Decomposition (LBD, Wang et al. 2025 §3.4).
 *
 * Given the item set I selected by an outer BMP solution whose own (unit,
 * x, orientation) choice failed 'y_check', LBD either finds *some* feasible
 * pattern using exactly I (any x-positions, not just the outer BMP's own
 * choice) or proves that none exists at all - the latter a strictly
 * stronger fact than the outer BMP's specific combination failing, since it
 * rules out every possible positioning of I at once, not just the one the
 * outer BMP happened to propose.
 *
 * Only ever called for 'Knapsack' (see the caller in
 * 'benders_decomposition_contiguity' below): for 'Feasibility', I is always
 * every unit in the instance (every item type mandatory), never a genuine
 * subset the BMP chose among several - LBD would then just recurse into an
 * equivalent, equally-sized copy of the very problem being solved, so the
 * caller cuts the specific failed combination directly instead.
 *
 * LBMP(I) (Wang et al. 2025 eq. (10a)-(10c): the same BMP MILP, restricted
 * to I with every unit now mandatory rather than optional, and with no
 * profit objective - a pure feasibility question) is exactly a fresh,
 * single-bin 'Feasibility' instance containing only the item types with a
 * unit in I, each with as many copies as I has units of it (Feasibility
 * resolves 'copies_min' to 'copies' automatically at 'build()' - see
 * 'InstanceBuilder::build()' - so every one of them is mandatory without
 * having to set it explicitly). This is therefore implemented as a direct
 * recursive call to 'benders_decomposition_contiguity' itself on that
 * sub-instance, rather than as its own separate
 * MILP-plus-manual-no-good-cut loop: the recursive call already provides
 * exactly the BMP/BSP loop (its own
 * no-good cuts, and, if it in turn gets stuck, its own nested LBD) LBMP(I)
 * needs, with nothing left here to reimplement. The bin type (dimensions,
 * resources, ...) is copied unchanged from 'instance' via the
 * 'add_bin_type(instance, id)' "copy from original" overload, which also
 * carries every relevant resource's consumption schedule over (see
 * 'add_item_type(instance, id)') - so this sub-instance remains subject to
 * the same resource constraints as 'instance' itself, even though the
 * outer BMP's own 'add_resource_constraints' rows already guarantee I
 * itself is resource-feasible (repositioning I can't change its resource
 * consumption).
 */
LbdResult run_lbd(
        const Instance& instance,
        const std::vector<Candidate>& item_set_units,
        const BendersDecompositionContiguityParameters& parameters)
{
    // Count how many units of each item type are in the item set, and
    // record, for each item type added to the sub-instance (in that same
    // order), which original item type it came from - needed to translate
    // the sub-instance's own solution back into 'instance''s own item type
    // ids below.
    std::vector<ItemPos> unit_counts(instance.number_of_item_types(), 0);
    for (const Candidate& unit: item_set_units)
        ++unit_counts[unit.item_type_id];

    InstanceBuilder lbmp_instance_builder;
    lbmp_instance_builder.set_objective(Objective::Feasibility);
    lbmp_instance_builder.set_parameters(instance.parameters());
    lbmp_instance_builder.add_bin_type(instance, 0);
    std::vector<ItemTypeId> sub_to_original_item_type_id;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (unit_counts[item_type_id] == 0)
            continue;
        ItemTypeId sub_item_type_id = lbmp_instance_builder.add_item_type(instance, item_type_id);
        lbmp_instance_builder.set_item_type_copies(sub_item_type_id, unit_counts[item_type_id]);
        sub_to_original_item_type_id.push_back(item_type_id);
    }
    Instance lbmp_instance = lbmp_instance_builder.build();

    BendersDecompositionContiguityParameters lbmp_parameters;
    lbmp_parameters.verbosity_level = 0;
    lbmp_parameters.timer = parameters.timer;
    lbmp_parameters.optimization_mode = parameters.optimization_mode;
    lbmp_parameters.use_tree_search = parameters.use_tree_search;
    lbmp_parameters.master_problem_milp_solver = parameters.master_problem_milp_solver;
    lbmp_parameters.master_problem_tree_search_guide_id = parameters.master_problem_tree_search_guide_id;
    lbmp_parameters.master_problem_tree_search_not_anytime_queue_size = parameters.master_problem_tree_search_not_anytime_queue_size;
    lbmp_parameters.seed = parameters.seed;
    BendersDecompositionContiguityOutput lbmp_output = benders_decomposition_contiguity(lbmp_instance, lbmp_parameters);

    LbdResult result(instance);
    if (lbmp_output.is_proven_infeasible) {
        result.status = LbdResult::Status::Infeasible;
        return result;
    }
    const Solution& lbmp_solution = lbmp_output.solution_pool.best();
    if (!lbmp_solution.feasible() || !lbmp_solution.full())
        return result;  // Inconclusive (timed out).

    result.status = LbdResult::Status::Feasible;
    result.solution.append_bin(lbmp_solution, 0, 1, {}, sub_to_original_item_type_id);
    return result;
}

}

BendersDecompositionContiguityOutput packingsolver::rectangle::benders_decomposition_contiguity(
        const Instance& instance,
        const BendersDecompositionContiguityParameters& parameters)
{
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility)
        throw std::invalid_argument(FUNC_SIGNATURE);
    if (instance.number_of_bin_types() != 1
            || instance.bin_type(0).copies != 1)
        throw std::invalid_argument(FUNC_SIGNATURE);

    BendersDecompositionContiguityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(0);

    std::vector<NoGoodCut> no_good_cuts;
    std::mt19937_64 generator(parameters.seed);

    for (output.number_of_iterations = 0;
            ;
            ++output.number_of_iterations) {
        if (parameters.not_anytime_maximum_number_of_iterations >= 0
                && output.number_of_iterations >= parameters.not_anytime_maximum_number_of_iterations) {
            break;
        }
        if (parameters.timer.needs_to_end())
            break;

        // The master problem is solved repeatedly, once per Benders
        // iteration - always as a bounded inner-loop building block, never
        // as a standalone anytime search (see 'onedimentional_contiguity::
        // TreeSearchParameters::optimization_mode'), regardless of this
        // algorithm's own 'optimization_mode'. Only the 'NotAnytimeSequential'
        // case is preserved as such (matching 'benders_decomposition.cpp''s
        // own sub-solves): everything else (including 'Anytime') maps to
        // 'NotAnytimeDeterministic'.
        OptimizationMode master_problem_optimization_mode
            = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
            OptimizationMode::NotAnytimeSequential:
            OptimizationMode::NotAnytimeDeterministic;

        OnedimensionalContiguityResult bmp_result;
        if (parameters.use_tree_search) {
            TreeSearchParameters tree_search_parameters;
            tree_search_parameters.timer = parameters.timer;
            tree_search_parameters.optimization_mode = master_problem_optimization_mode;
            tree_search_parameters.guide_id = parameters.master_problem_tree_search_guide_id;
            tree_search_parameters.not_anytime_queue_size = parameters.master_problem_tree_search_not_anytime_queue_size;
            bmp_result = tree_search(instance, no_good_cuts, tree_search_parameters);
        } else {
            MilpParameters milp_parameters;
            milp_parameters.timer = parameters.timer;
            milp_parameters.optimization_mode = master_problem_optimization_mode;
            milp_parameters.solver = parameters.master_problem_milp_solver;
            bmp_result = milp(instance, no_good_cuts, milp_parameters);
        }

        if (parameters.timer.needs_to_end())
            break;

        if (bmp_result.status == OnedimensionalContiguityResult::Status::Infeasible) {
            // The relaxation itself has no feasible solution (including
            // every cut added so far, each a sound necessary condition):
            // the original instance has none either.
            algorithm_formatter.update_is_proven_infeasible();
            break;
        }
        if (bmp_result.status == OnedimensionalContiguityResult::Status::Inconclusive) {
            // The master gave up (timed out, or - for the tree-search
            // master in non-'Anytime' mode - its fixed beam width wasn't
            // wide enough to be exhaustive) without proving there is no
            // feasible selection either: neither a solution nor
            // infeasibility can be soundly reported, so stop without
            // claiming either.
            break;
        }

        if (instance.objective() == Objective::Knapsack)
            algorithm_formatter.update_knapsack_bound(bmp_result.objective_value);

        if (parameters.timer.needs_to_end())
            break;

        std::vector<YCheckUnit> y_check_units;
        for (const Candidate& candidate: bmp_result.selected_units)
            y_check_units.push_back({candidate.x, candidate.width, candidate.height});
        YCheckResult y_check_result;
        if (y_check_units.empty()) {
            y_check_result.status = YCheckStatus::Feasible;
        } else {
            y_check_result = y_check(bin_type.rect.x, bin_type.rect.y, y_check_units, parameters.timer);
        }

        if (y_check_result.status == YCheckStatus::Inconclusive) {
            // y-check is strongly NP-complete (Côté, Dell'Amico & Iori
            // 2014): the search gave up (timed out) without either finding
            // a feasible y-assignment for this BMP proposal or proving
            // none exists, so neither a solution nor a no-good cut can be
            // soundly derived from it - stop without claiming either.
            break;
        }

        if (y_check_result.status == YCheckStatus::Feasible) {
            SolutionBuilder solution_builder(instance);
            if (!bmp_result.selected_units.empty()) {
                BinPos bin_pos = solution_builder.add_bin(0, 1);
                for (size_t index = 0; index < bmp_result.selected_units.size(); ++index) {
                    const Candidate& candidate = bmp_result.selected_units[index];
                    solution_builder.add_item(
                            bin_pos,
                            candidate.item_type_id,
                            {candidate.x, y_check_result.y[index]},
                            candidate.rotate);
                }
            }
            Solution solution = solution_builder.build();
            std::stringstream ss;
            ss << "BDC it " << output.number_of_iterations;
            algorithm_formatter.update_solution(solution, ss.str());
            break;
        }

        if (instance.objective() == Objective::Knapsack) {
            // The BMP's exact (unit, x, orientation) choice is infeasible,
            // but a different x-positioning of the very same item *subset*
            // might still work (Wang et al. 2025 §3.4): escalate to LBD
            // before giving up on this subset entirely, rather than
            // cutting it off right away. Only meaningful when the item set
            // is genuinely a subset the BMP chose to select - see the
            // 'Feasibility' branch below for why it is skipped there.
            LbdResult lbd_result = run_lbd(
                    instance, bmp_result.selected_units, parameters);

            if (lbd_result.status == LbdResult::Status::Inconclusive)
                break;

            if (lbd_result.status == LbdResult::Status::Feasible) {
                std::stringstream ss;
                ss << "BDC it " << output.number_of_iterations << " (LBD)";
                algorithm_formatter.update_solution(lbd_result.solution, ss.str());
                break;
            }

            // LBD proved that no positioning of this item set is feasible
            // at all - a strictly stronger fact than the BMP's own (unit,
            // x, orientation) combination failing on its own, so cut off
            // the whole item set at any position (Wang et al. 2025's
            // §3.1.1 covering cut) rather than just that one combination -
            // a positional cut alone would leave the BMP free to keep
            // proposing other positionings of this same already-proven-
            // infeasible item set, each needing its own (now recursive)
            // call to 'run_lbd' to re-derive the same fact.
            no_good_cuts.push_back(build_covering_cut(
                    instance, bmp_result.selected_units));
        } else {
            // 'Feasibility' always sets 'copies_min == copies' (see
            // 'onedimentional_contiguity::milp'/'::tree_search'), so the
            // BMP's own item set is never a genuine *subset* choice - it is
            // always every unit in the instance. LBD's own sub-instance
            // ('run_lbd') would then be, up to the no-good cuts accumulated
            // so far at this level (which it does not inherit), essentially
            // 'instance' itself: escalating to it would recurse into an
            // equivalent, equally-sized copy of this very problem - at best
            // redundant (this loop's own no-good cuts already retry
            // different positionings of the same, already-mandatory item
            // set directly), at worst compounding into unboundedly deep
            // recursion if that copy also needs to escalate. Cut off just
            // the specific failed (unit, x, orientation) combination
            // instead - this is exactly LBMP's own inner loop (Wang et al.
            // 2025 §3.4/§3.5.2), so its own cut strengthening procedure
            // applies here directly: shrink the infeasible unit set (steps
            // (1)-(3): binary split, line sweep, 12-scan iterative removal)
            // and lift the surviving units' own forbidden windows via LP
            // (step (4)) before cutting, rather than the raw,
            // single-position
            // 'onedimentional_contiguity::build_positional_no_good_cut'.
            std::vector<Candidate> shrunk = shrink_via_split(
                    bin_type.rect.x, bin_type.rect.y,
                    bmp_result.selected_units, parameters.timer);
            shrunk = shrink_via_sweep(
                    bin_type.rect.x, bin_type.rect.y, std::move(shrunk), parameters.timer);
            shrunk = shrink_via_removal(
                    bin_type.rect.x, bin_type.rect.y, instance,
                    std::move(shrunk), generator, parameters.timer);
            no_good_cuts.push_back(build_lifted_cut(
                    bin_type.rect.x, instance, shrunk, parameters));
        }
    }

    algorithm_formatter.end();
    return output;
}
