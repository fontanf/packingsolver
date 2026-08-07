/**
 * The meet-in-the-middle principle for cutting and packing problems
 *
 * Côté & Iori (2018), "The Meet-in-the-Middle Principle for Cutting and
 * Packing Problems", INFORMS Journal on Computing 30(4):646-661.
 *
 * A generic technique to reduce the set of candidate positions along a
 * single axis (a bin of a given "width", to be packed/cut with items of
 * given "widths") that need to be considered without losing optimality -
 * used here to shrink the number of x-position variables the rectangle
 * domain's bar-relaxation-master Benders decomposition ('benders_
 * decomposition_bar.cpp') generates per item, but domain- and
 * axis-agnostic otherwise (it only ever reasons about plain widths and a
 * capacity, on one axis at a time), so equally applicable to any other
 * domain/algorithm that enumerates 1D positions - including higher-
 * dimensional domains with several allowed rotations per item (e.g. up to
 * six for boxes), which is why every item here is a *list* of widths (one
 * per rotation) rather than a single one - see "Rotations" below.
 *
 * Background - normal patterns (Herz 1972; Christofides & Whitlock 1977):
 * every optimal solution to a 1D packing/cutting problem has an equivalent
 * one where every item's position, along a given axis, is a "normal
 * pattern" of the *other* items' widths - i.e. expressible as a subset sum
 * of their widths (the empty subset, position 0, included). This already
 * shrinks the search space (from every integer position to just the O(nW)
 * distinct subset sums), but its reduction weakens as the number of items
 * and the bin width grow: for large positions there are exponentially many
 * subsets summing to (approximately) that position, so almost every
 * position ends up being a normal pattern anyway.
 *
 * The meet-in-the-middle (MIM) idea: instead of a single set of "push
 * everything to the left" positions, fix a threshold 't' and require every
 * item positioned before 't' to be pushed as far left as normal patterns
 * allow, and every item positioned at or after 't' to be pushed as far
 * *right* as normal patterns allow (i.e. normal patterns of the *mirrored*
 * problem, reflected back). Côté & Iori prove this still preserves
 * optimality for *any* fixed threshold 't', that the resulting pattern set
 * is never larger than the plain normal-pattern set, and that - since the
 * two forms of pushing prune independently from either side - it is
 * frequently smaller in practice (their tests: often around 50%, and over
 * 70% on some benchmark instances). 'minimal_mim_patterns' below picks,
 * for each item, whichever threshold minimizes its own pattern count.
 *
 * Rotations. An item that may be packed in several rotations presents a
 * *different* width along this axis per rotation (e.g. a rectangle
 * rotated 90 degrees, or one of up to six axis-aligned orientations for a
 * box). This is modeled by giving every item a *list* of widths - one per
 * allowed rotation - throughout this file, and computing subset sums as a
 * *multiple-choice* knapsack: an item contributes at most one of its own
 * widths to any sum, never zero-or-more of each independently. This
 * matters for soundness, not just precision: naively treating each
 * rotation as if it were its own, separate item (i.e. plain 0-1 subset
 * sums with one list entry per rotation) lets a subset "use" two different
 * rotations of the very same physical item at once - a combination that
 * can never occur in an actual packing, since a single item is only ever
 * in one rotation at a time. That over-generates candidate positions
 * (still safe as an upper bound, see 'benders_decomposition_contiguity.cpp''s
 * history, but needlessly loose): the multiple-choice formulation computes
 * the tight set directly, at the same asymptotic complexity (a constant
 * factor of "number of rotations" per item).
 */

#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <algorithm>

namespace packingsolver
{

/**
 * Compute the normal patterns of a set of items, each given as a list of
 * widths (one per allowed rotation - a single-element list for an item
 * that cannot rotate), in a bin of the given capacity (a generalization,
 * to multiple choices per item, of Algorithm 1, NormalPatterns(I;W), of
 * Côté & Iori 2018): every position expressible as a subset sum in which
 * each item contributes at most one of its own listed widths (see
 * "Rotations" in the file-level comment above) - via a multiple-choice
 * subset-sum dynamic program, O(sum of every item's number of rotations *
 * capacity) time, O(capacity) space. The result is sorted and
 * duplicate-free.
 *
 * Per the normal-patterns property (see the file-level comment above),
 * this should be called with 'item_widths' excluding whichever item is
 * being positioned - not the full item set.
 */
inline std::vector<Length> normal_patterns(
        const std::vector<std::vector<Length>>& item_widths,
        Length capacity)
{
    if (capacity < 0)
        return {};

    std::vector<bool> reachable(capacity + 1, false);
    reachable[0] = true;
    for (const std::vector<Length>& rotation_widths: item_widths) {
        // Every rotation of this item is checked against the *same*,
        // not-yet-updated-by-this-item 'reachable' state (read before any
        // of them writes), for every position visited in this downward
        // pass: exactly the standard 0-1 knapsack "iterate the capacity
        // downward" trick, extended to let any one of several widths
        // stand in for "use this item", rather than only one fixed width.
        for (Length p = capacity; p >= 0; --p) {
            if (reachable[p])
                continue;
            for (Length width: rotation_widths) {
                if (width > 0 && width <= p && reachable[p - width]) {
                    reachable[p] = true;
                    break;
                }
            }
        }
    }

    std::vector<Length> patterns;
    for (Length p = 0; p <= capacity; ++p) {
        if (reachable[p])
            patterns.push_back(p);
    }
    return patterns;
}

/**
 * Compute the MIM pattern set of a single item (already resolved to one
 * specific rotation, hence the single 'item_width'), for a given threshold
 * 't' (a generalization, to multiple choices per *other* item, of
 * Algorithm 2, MIMPatterns(I;i;W;t), of Côté & Iori 2018): every position
 * at which the item may be packed without losing optimality, given that
 * positions strictly before 't' are reached by pushing items to the left
 * (a normal pattern of 'other_item_widths') and positions from 't' onward
 * are reached by pushing items to the right (a normal pattern of the
 * mirrored problem, reflected back) - see the file-level comment above.
 * The result is sorted and duplicate-free.
 *
 * 'other_item_widths' excludes the item being positioned entirely (every
 * rotation of it, not just the one given by 'item_width' - see
 * "Rotations" in the file-level comment above); 'item_width' and
 * 'capacity' are the item's own (already-chosen) width and the bin's
 * capacity along this axis. 'threshold' must be in '[1, capacity]' (see
 * 'minimal_mim_patterns' below for how to pick it).
 */
inline std::vector<Length> mim_patterns(
        const std::vector<std::vector<Length>>& other_item_widths,
        Length item_width,
        Length capacity,
        Length threshold)
{
    std::vector<Length> patterns = normal_patterns(
            other_item_widths,
            std::min(threshold - 1, capacity - item_width));

    for (Length p: normal_patterns(other_item_widths, capacity - item_width - threshold))
        patterns.push_back(capacity - item_width - p);

    std::sort(patterns.begin(), patterns.end());
    patterns.erase(std::unique(patterns.begin(), patterns.end()), patterns.end());
    return patterns;
}

/**
 * Remove every pattern dominated by an earlier one (Côté & Iori 2018,
 * Proposition 7): given 'positions_and_widths' - pairs '(position,
 * effective_width)', e.g. the output of the widening step below - sorted
 * by position, drop any entry 's' for which an earlier entry 'p' (p < s)
 * already reaches at least as far ('s + width_s <= p + width_p'): packing
 * the item at 'p' then covers everything packing it at 's' would, so 's'
 * is never needed. Domination is transitive here (if 'p' dominates 's' and
 * is itself later dominated by some earlier 'p2', then 'p2' reaches at
 * least as far as 'p' and therefore at least as far as 's' too), so a
 * single pass comparing every pair against the original set is enough -
 * no need to re-check pairs after a removal.
 */
inline std::vector<Length> prune_dominated_patterns(
        const std::vector<std::pair<Length, Length>>& positions_and_widths)
{
    size_t n = positions_and_widths.size();
    std::vector<bool> dominated(n, false);
    for (size_t i = 0; i < n; ++i) {
        Length reach_i = positions_and_widths[i].first + positions_and_widths[i].second;
        for (size_t j = 0; j < n; ++j) {
            if (positions_and_widths[j].first <= positions_and_widths[i].first)
                continue;
            Length reach_j = positions_and_widths[j].first + positions_and_widths[j].second;
            if (reach_j <= reach_i)
                dominated[j] = true;
        }
    }
    std::vector<Length> result;
    for (size_t i = 0; i < n; ++i)
        if (!dominated[i])
            result.push_back(positions_and_widths[i].first);
    return result;
}

/**
 * One (item, rotation) combination - a single width value, together with
 * where it came from - used internally by 'minimal_mim_patterns' below to
 * iterate over every combination while still being able to group them back
 * by item (e.g. to exclude an entire item, every one of its rotations
 * alike, from another item's "other widths").
 */
struct MimVariant
{
    size_t item_id;
    size_t rotation_id;
    Length width;
};

/**
 * Compute the minimal MIM pattern set of every (item, rotation) - see
 * "Rotations" in the file-level comment above (Côté & Iori 2018,
 * §3.1-3.3): for each item and each of its allowed rotations, its own
 * reduced set of positions - never larger, and in practice frequently
 * about half the size, of its normal patterns (see the file-level comment
 * above). Combines three techniques from the paper, applied in sequence:
 * - Algorithm 3 (MinimalMIMSet(I;W)), §3.1-3.2: pick the single threshold
 *   that minimizes the *total* number of patterns summed over every
 *   (item, rotation).
 * - Preprocessing 1, §3.3 (Proposition 5): the (item, rotation) achieving
 *   the smallest width overall never needs *both* its left and right
 *   pattern sets - for any threshold 't', there is always an optimal
 *   solution where it ends up on one threshold-determined side only, so
 *   the other side is skipped for it entirely.
 * - Preprocessing 2, §3.3 (Propositions 6-7): every (item, rotation)'s
 *   patterns are first "widened" - since no other valid pattern can exist
 *   strictly between its natural edge and the next real pattern position
 *   (by the MIM principle itself), it can be pretended wide enough to
 *   reach that next position "for free", without changing feasibility;
 *   right patterns are additionally moved as far left as that same
 *   argument allows, using every *other item's* own (per-rotation)
 *   pattern sets to find how close another item's right edge could really
 *   get. Once every pattern has this "enlarged" width, a later pattern
 *   that never reaches any farther than an earlier one is redundant and
 *   dropped.
 *
 * 'widths[item_id]' lists item 'item_id''s width under each of its
 * allowed rotations (a single-element list for an item that cannot
 * rotate). Returned as 'result[item_id][rotation_id]', in the same order
 * as 'widths', each entry sorted and duplicate-free.
 *
 * O(m^2 * capacity) time for the threshold selection and the widening of
 * left patterns, where 'm' is the total number of (item, rotation)
 * combinations (the same complexity as computing the plain normal
 * patterns of every one of them individually); the cross-item widening of
 * right patterns (Preprocessing 2, part B) is the most expensive step, at
 * O(m^2 * capacity^2) in the worst case (for every (item, rotation), every
 * one of its O(capacity) right patterns scans every other item's every
 * rotation's O(capacity) own patterns) - acceptable for a step run once
 * per bin rather than per search node, but worth revisiting (e.g. via a
 * prefix-max lookup per item) if it ever shows up as a bottleneck on very
 * wide bins.
 */
inline std::vector<std::vector<std::vector<Length>>> minimal_mim_patterns(
        const std::vector<std::vector<Length>>& widths,
        Length capacity)
{
    size_t number_of_items = widths.size();
    std::vector<std::vector<std::vector<Length>>> result(number_of_items);
    for (size_t item_id = 0; item_id < number_of_items; ++item_id)
        result[item_id].assign(widths[item_id].size(), {});
    if (capacity <= 0)
        return result;

    std::vector<MimVariant> variants;
    for (size_t item_id = 0; item_id < number_of_items; ++item_id)
        for (size_t rotation_id = 0; rotation_id < widths[item_id].size(); ++rotation_id)
            variants.push_back({item_id, rotation_id, widths[item_id][rotation_id]});
    if (variants.empty())
        return result;

    // 'other_item_widths_excluding(item_id)' - every OTHER item's full,
    // per-rotation width list (an entire item is excluded at once, every
    // rotation alike - see 'MimVariant' above - never just the one
    // rotation a given variant happens to share a width with).
    auto other_item_widths_excluding = [&widths](size_t excluded_item_id)
    {
        std::vector<std::vector<Length>> other_item_widths;
        other_item_widths.reserve(widths.size() - 1);
        for (size_t item_id = 0; item_id < widths.size(); ++item_id)
            if (item_id != excluded_item_id)
                other_item_widths.push_back(widths[item_id]);
        return other_item_widths;
    };

    // 'variant_normal_patterns[v]' is variant 'v''s own 'B_i' (using the
    // multiple-choice 'normal_patterns' over every OTHER item), capped at
    // 'capacity - variants[v].width' - empty if that rotation does not fit
    // the bin at all.
    std::vector<std::vector<Length>> variant_normal_patterns(variants.size());
    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        const MimVariant& variant = variants[variant_id];
        Length variant_capacity = capacity - variant.width;
        if (variant_capacity < 0)
            continue;
        variant_normal_patterns[variant_id] = normal_patterns(
                other_item_widths_excluding(variant.item_id), variant_capacity);
    }

    // Threshold selection (Algorithm 3, generalized to sum over every
    // (item, rotation) rather than every item) - see the single-choice
    // version of this file for the detailed reasoning; unchanged here
    // beyond iterating over 'variants' instead of items directly.
    std::vector<Counter> left_count(capacity + 1, 0);
    std::vector<Counter> right_count(capacity + 1, 0);
    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        for (Length p: variant_normal_patterns[variant_id]) {
            left_count[p] += 1;
            right_count[capacity - variants[variant_id].width - p] += 1;
        }
    }
    for (Length p = 1; p <= capacity; ++p) {
        left_count[p] += left_count[p - 1];
        right_count[capacity - p] += right_count[capacity - (p - 1)];
    }

    Length threshold = 1;
    Counter threshold_total = left_count[0] + right_count[1];
    for (Length p = 2; p <= capacity; ++p) {
        Counter total = left_count[p - 1] + right_count[p];
        if (total < threshold_total) {
            threshold_total = total;
            threshold = p;
        }
    }

    // Split every variant's patterns into its left and right sets at
    // 'threshold' (still §3.1-3.2, not yet the §3.3 preprocessing below).
    std::vector<std::vector<Length>> left(variants.size());
    std::vector<std::vector<Length>> right(variants.size());
    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        const MimVariant& variant = variants[variant_id];
        for (Length p: variant_normal_patterns[variant_id]) {
            if (p < threshold)
                left[variant_id].push_back(p);
            Length right_value = capacity - variant.width - p;
            if (right_value >= threshold)
                right[variant_id].push_back(right_value);
        }
        // Already ascending ('variant_normal_patterns[variant_id]' is
        // sorted and both transformations above are monotonic in 'p'),
        // just dedupe.
        left[variant_id].erase(std::unique(left[variant_id].begin(), left[variant_id].end()), left[variant_id].end());
        std::sort(right[variant_id].begin(), right[variant_id].end());
        right[variant_id].erase(std::unique(right[variant_id].begin(), right[variant_id].end()), right[variant_id].end());
    }

    // Preprocessing 1 (Proposition 5): the globally smallest-width variant
    // is forced onto a single, threshold-determined side, so the other
    // side can be dropped for it without ever being needed.
    {
        size_t min_width_variant_id = 0;
        for (size_t variant_id = 1; variant_id < variants.size(); ++variant_id)
            if (variants[variant_id].width < variants[min_width_variant_id].width)
                min_width_variant_id = variant_id;
        // ceil((capacity - width) / 2) - correct as long as the numerator
        // is non-negative, which it is whenever the variant actually fits
        // the bin; when it doesn't (a degenerate case), this variant's
        // 'left'/'right' are already both empty, so clearing either below
        // is a no-op regardless of what 'half' computes to.
        Length min_width = variants[min_width_variant_id].width;
        Length half = (capacity - min_width + 1) / 2;
        if (threshold <= half) {
            left[min_width_variant_id].clear();
        } else {
            right[min_width_variant_id].clear();
        }
    }

    // Preprocessing 2, part A (Propositions 6A and 7): widen, then prune,
    // every variant's own left patterns - self-contained, only ever
    // referencing that variant's own (post-Preprocessing-1) pattern set.
    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        if (left[variant_id].empty())
            continue;
        std::vector<Length> own_positions = left[variant_id];
        own_positions.insert(own_positions.end(), right[variant_id].begin(), right[variant_id].end());
        std::sort(own_positions.begin(), own_positions.end());

        std::vector<std::pair<Length, Length>> widened;
        widened.reserve(left[variant_id].size());
        for (Length p: left[variant_id]) {
            Length q = capacity;
            for (Length s: own_positions) {
                if (s > p && s >= p + variants[variant_id].width) {
                    q = s;
                    break;
                }
            }
            widened.push_back({p, q - p});
        }
        left[variant_id] = prune_dominated_patterns(widened);
    }

    // Preprocessing 2, part B (Propositions 6B and 7): widen every
    // variant's right patterns by moving each one as far left as no
    // *other item's* own pattern set (any of ITS rotations, each with its
    // own width - see "Rotations" in the file-level comment above) could
    // possibly reach (using their true, unenlarged widths - the same
    // 'own_positions' snapshots below are taken once, upfront, since part
    // A only changes *which* left patterns survive, never any variant's
    // true width), then prune the result. Distinct original positions can
    // end up moved to the same new position; the largest enlarged width
    // proven for it is kept (each is independently sound, so the largest
    // remains sound too).
    std::vector<std::vector<Length>> own_positions_before_part_b(variants.size());
    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        std::vector<Length>& positions = own_positions_before_part_b[variant_id];
        positions = left[variant_id];
        positions.insert(positions.end(), right[variant_id].begin(), right[variant_id].end());
        std::sort(positions.begin(), positions.end());
    }
    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        if (right[variant_id].empty())
            continue;
        const MimVariant& variant = variants[variant_id];
        std::vector<std::pair<Length, Length>> moved;  // (new position, enlarged width)
        for (Length p: right[variant_id]) {
            Length q = 0;
            for (size_t other_variant_id = 0; other_variant_id < variants.size(); ++other_variant_id) {
                if (variants[other_variant_id].item_id == variant.item_id)
                    continue;
                for (Length s: own_positions_before_part_b[other_variant_id]) {
                    Length reach = s + variants[other_variant_id].width;
                    if (reach <= p && reach > q)
                        q = reach;
                }
            }
            Length enlarged_width = p + variant.width - q;
            bool found = false;
            for (std::pair<Length, Length>& entry: moved) {
                if (entry.first == q) {
                    entry.second = std::max(entry.second, enlarged_width);
                    found = true;
                    break;
                }
            }
            if (!found)
                moved.push_back({q, enlarged_width});
        }
        std::sort(moved.begin(), moved.end());
        right[variant_id] = prune_dominated_patterns(moved);
    }

    for (size_t variant_id = 0; variant_id < variants.size(); ++variant_id) {
        const MimVariant& variant = variants[variant_id];
        std::vector<Length>& variant_result = result[variant.item_id][variant.rotation_id];
        variant_result = left[variant_id];
        variant_result.insert(variant_result.end(), right[variant_id].begin(), right[variant_id].end());
        std::sort(variant_result.begin(), variant_result.end());
        variant_result.erase(std::unique(variant_result.begin(), variant_result.end()), variant_result.end());
    }
    return result;
}

}
