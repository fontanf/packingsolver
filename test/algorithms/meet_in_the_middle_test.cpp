#include "algorithms/meet_in_the_middle.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <random>
#include <sstream>

using namespace packingsolver;

namespace
{

/** Wrap a flat width list into one single-rotation item per width. */
std::vector<std::vector<Length>> single_rotation(const std::vector<Length>& widths)
{
    std::vector<std::vector<Length>> item_widths;
    for (Length width: widths)
        item_widths.push_back({width});
    return item_widths;
}

}

TEST(MeetInTheMiddle, NormalPatternsExample1)
{
    // Côté & Iori (2018), Example 1: W = 27, widths = (5, 10, 12, 15).
    std::vector<Length> patterns = normal_patterns(single_rotation({5, 10, 12, 15}), 27);
    std::vector<Length> expected = {0, 5, 10, 12, 15, 17, 20, 22, 25, 27};
    EXPECT_EQ(patterns, expected);
}

TEST(MeetInTheMiddle, NormalPatternsMultipleChoiceExcludesUsingBothRotationsAtOnce)
{
    // Item A: widths {3, 5} (rotatable); item B: width {4}. A single-choice
    // (wrong) subset sum would also reach 8 = 3 + 5 by using BOTH of A's
    // rotations in the same subset - impossible in a real packing, since a
    // physical item is only ever in one rotation at a time. The
    // multiple-choice formulation must exclude it.
    std::vector<std::vector<Length>> item_widths = {{3, 5}, {4}};
    std::vector<Length> patterns = normal_patterns(item_widths, 12);
    std::vector<Length> expected = {0, 3, 4, 5, 7, 9};
    EXPECT_EQ(patterns, expected);
    EXPECT_EQ(std::find(patterns.begin(), patterns.end(), 8), patterns.end())
        << "8 (= 3 + 5, both of item A's rotations at once) must not appear";
}

TEST(MeetInTheMiddle, NormalPatternsMultipleChoiceTighterThanFlattening)
{
    // The same instance, but demonstrating the concrete looseness that
    // treating each rotation as an independent item (the wrong way to
    // handle rotations - flattening) introduces: computing item A's OWN
    // left-pattern set (rotation width 3, so "other" width to consider is
    // only item B's 4) must not include positions only reachable by
    // (incorrectly) treating item A's own *other* rotation (width 5) as if
    // it were a second, independent item available to combine with B.
    std::vector<std::vector<Length>> other_correct = {{4}};       // just item B
    std::vector<std::vector<Length>> other_flattened = {{5}, {4}}; // wrongly includes A's rotation 5 too

    std::vector<Length> correct = normal_patterns(other_correct, 9);
    std::vector<Length> flattened = normal_patterns(other_flattened, 9);

    EXPECT_EQ(correct, (std::vector<Length>{0, 4}));
    // The flattened (wrong) computation spuriously reaches 5 and 9 by
    // using item A's own other rotation as a stand-in "other item".
    EXPECT_EQ(flattened, (std::vector<Length>{0, 4, 5, 9}));
}

TEST(MeetInTheMiddle, MimPatternsExample1Item1)
{
    // Item 1 (width 5) excluded, others = (10, 12, 15), threshold t = 10.
    // Côté & Iori (2018) work this one out explicitly: L_{1,10} = {0},
    // R_{1,10} = {10, 12, 22}, so M_{1,10} = {0, 10, 12, 22}.
    std::vector<Length> patterns = mim_patterns(single_rotation({10, 12, 15}), 5, 27, 10);
    std::vector<Length> expected = {0, 10, 12, 22};
    EXPECT_EQ(patterns, expected);
}

TEST(MeetInTheMiddle, MimPatternsExample1Item2)
{
    // Item 2 (width 10) excluded, others = (5, 12, 15), threshold t = 10.
    // Côté & Iori (2018): M_{2,10} = {0, 5, 12, 17}.
    std::vector<Length> patterns = mim_patterns(single_rotation({5, 12, 15}), 10, 27, 10);
    std::vector<Length> expected = {0, 5, 12, 17};
    EXPECT_EQ(patterns, expected);
}

TEST(MeetInTheMiddle, MimPatternsExample1Item4)
{
    // Item 4 (width 15) excluded, others = (5, 10, 12), threshold t = 10.
    // Côté & Iori (2018): M_{4,10} = {0, 5, 12}.
    std::vector<Length> patterns = mim_patterns(single_rotation({5, 10, 12}), 15, 27, 10);
    std::vector<Length> expected = {0, 5, 12};
    EXPECT_EQ(patterns, expected);
}

TEST(MeetInTheMiddle, MimPatternsNeverLargerThanNormalPatterns)
{
    // Proposition 3 of Côté & Iori (2018): |M_it| <= |B_i| for every
    // threshold t, where B_i is the plain normal-pattern set of the other
    // items (i.e. 'mim_patterns' evaluated at the "degenerate" threshold
    // t = capacity, per Proposition 2 - see 'MimPatternsAtCapacityMatchesNormalPatterns').
    std::vector<Length> widths = {3, 4, 6, 7, 9, 11};
    Length capacity = 40;
    for (size_t excluded = 0; excluded < widths.size(); ++excluded) {
        std::vector<Length> other_widths;
        for (size_t i = 0; i < widths.size(); ++i)
            if (i != excluded)
                other_widths.push_back(widths[i]);
        Length item_width = widths[excluded];
        std::vector<Length> normal = normal_patterns(single_rotation(other_widths), capacity - item_width);
        for (Length threshold = 1; threshold <= capacity; ++threshold) {
            std::vector<Length> mim = mim_patterns(single_rotation(other_widths), item_width, capacity, threshold);
            EXPECT_LE(mim.size(), normal.size());
        }
    }
}

TEST(MeetInTheMiddle, MimPatternsAtCapacityMatchesNormalPatterns)
{
    // Proposition 2 of Côté & Iori (2018): the MIM pattern set for
    // threshold t = capacity is exactly the plain normal-pattern set (the
    // right-pattern set is empty, since 'capacity - item_width - t' < 0).
    std::vector<Length> widths = {5, 10, 12, 15};
    Length capacity = 27;
    for (size_t excluded = 0; excluded < widths.size(); ++excluded) {
        std::vector<Length> other_widths;
        for (size_t i = 0; i < widths.size(); ++i)
            if (i != excluded)
                other_widths.push_back(widths[i]);
        Length item_width = widths[excluded];
        std::vector<Length> normal = normal_patterns(single_rotation(other_widths), capacity - item_width);
        std::vector<Length> mim = mim_patterns(single_rotation(other_widths), item_width, capacity, capacity);
        EXPECT_EQ(mim, normal);
    }
}

TEST(MeetInTheMiddle, MinimalMimPatternsMatchesBestThreshold)
{
    // Cross-check 'minimal_mim_patterns' (which finds the threshold
    // minimizing the *total* pattern count via the O(n^2 * capacity)
    // incremental algorithm) against a brute-force search over every
    // threshold for each item independently via 'mim_patterns' - the
    // total achieved by 'minimal_mim_patterns' must be at most the total
    // for every single threshold applied uniformly to every item.
    std::vector<Length> widths = {3, 4, 6, 7, 9, 11, 13};
    Length capacity = 45;

    std::vector<std::vector<std::vector<Length>>> minimal = minimal_mim_patterns(single_rotation(widths), capacity);
    ASSERT_EQ(minimal.size(), widths.size());
    size_t minimal_total = 0;
    for (const std::vector<std::vector<Length>>& item_patterns: minimal)
        for (const std::vector<Length>& patterns: item_patterns)
            minimal_total += patterns.size();

    for (Length threshold = 1; threshold <= capacity; ++threshold) {
        size_t total = 0;
        for (size_t excluded = 0; excluded < widths.size(); ++excluded) {
            std::vector<Length> other_widths;
            for (size_t i = 0; i < widths.size(); ++i)
                if (i != excluded)
                    other_widths.push_back(widths[i]);
            total += mim_patterns(single_rotation(other_widths), widths[excluded], capacity, threshold).size();
        }
        EXPECT_LE(minimal_total, total);
    }
}

TEST(MeetInTheMiddle, MinimalMimPatternsContainsOnlyValidPositions)
{
    // Every returned position must actually leave room for the item
    // (0 <= p <= capacity - width), and every item that does not fit the
    // bin at all must get an empty pattern set.
    std::vector<Length> widths = {5, 30, 12, 8};
    Length capacity = 27;

    std::vector<std::vector<std::vector<Length>>> minimal = minimal_mim_patterns(single_rotation(widths), capacity);
    ASSERT_EQ(minimal.size(), widths.size());
    for (size_t item_id = 0; item_id < widths.size(); ++item_id) {
        ASSERT_EQ(minimal[item_id].size(), 1u);
        if (widths[item_id] > capacity) {
            EXPECT_TRUE(minimal[item_id][0].empty());
            continue;
        }
        for (Length p: minimal[item_id][0]) {
            EXPECT_GE(p, 0);
            EXPECT_LE(p, capacity - widths[item_id]);
        }
    }
}

// The following three tests pin down 'minimal_mim_patterns''s exact output
// (including the §3.3 preprocessing - Proposition 5's "skip the unused
// side" for the minimum-width item, and Propositions 6-7's widen-then-
// prune passes); 'MinimalMimPatternsPreserveTwoDimensionalFeasibility'
// below checks that they preserve feasibility. widths = (6, 4, 3, 7) in
// W = 20 exercises the left-pattern widen+prune (Preprocessing 2, part A -
// item 0's normal pattern 3 is not kept); widths = (8, 5, 6) in W = 20
// exercises the right-pattern
// cross-item widen+move (part B - item 0's right pattern at 12 moves to 11,
// widened from 8 to 9 so its right border stays at 20); Example 1 from the
// paper itself (widths = (5, 10, 12, 15), W = 27) is the same instance
// already used to validate the unpreprocessed 'mim_patterns' above, now
// checked all the way through the preprocessing too. All three use
// single-rotation items, to isolate the preprocessing logic from the
// multiple-choice generalization (checked separately below).
TEST(MeetInTheMiddle, MinimalMimPatternsPreprocessingLeftWidenAndPrune)
{
    std::vector<std::vector<std::vector<Length>>> result = minimal_mim_patterns(single_rotation({6, 4, 3, 7}), 20);
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0][0], (std::vector<Length>{0, 4, 7, 10, 11, 14}));
    EXPECT_EQ(result[1][0], (std::vector<Length>{0, 6, 7, 9, 10, 13, 16}));
    EXPECT_EQ(result[2][0], (std::vector<Length>{4, 6, 7, 10, 11, 13, 17}));
    EXPECT_EQ(result[3][0], (std::vector<Length>{0, 4, 6, 7, 9, 10, 13}));
}

TEST(MeetInTheMiddle, MinimalMimPatternsPreprocessingRightWidenAndMove)
{
    std::vector<std::vector<std::vector<Length>>> result = minimal_mim_patterns(single_rotation({8, 5, 6}), 20);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0][0], (std::vector<Length>{0, 6, 12}));
    EXPECT_EQ(result[1][0], (std::vector<Length>{6, 8, 15}));
    EXPECT_EQ(result[2][0], (std::vector<Length>{0, 8, 14}));
}

TEST(MeetInTheMiddle, MinimalMimPatternsPreprocessingExample1)
{
    std::vector<std::vector<std::vector<Length>>> result = minimal_mim_patterns(single_rotation({5, 10, 12, 15}), 27);
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0][0], (std::vector<Length>{10, 12, 22}));
    EXPECT_EQ(result[1][0], (std::vector<Length>{0, 5, 12, 17}));
    EXPECT_EQ(result[2][0], (std::vector<Length>{0, 5, 10, 15}));
    EXPECT_EQ(result[3][0], (std::vector<Length>{0, 5, 12}));
}

TEST(MeetInTheMiddle, MinimalMimPatternsWithRotationsMatchesSingleRotationWhenOnlyOneChoice)
{
    // A pure sanity check that the multiple-choice generalization reduces
    // exactly to the single-rotation results above when every item has
    // only one width - i.e. that adding rotation support did not change
    // anything for the (still overwhelmingly common) non-rotatable case.
    std::vector<std::vector<std::vector<Length>>> result = minimal_mim_patterns(single_rotation({6, 4, 3, 7}), 20);
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0][0], (std::vector<Length>{0, 4, 7, 10, 11, 14}));
    EXPECT_EQ(result[1][0], (std::vector<Length>{0, 6, 7, 9, 10, 13, 16}));
    EXPECT_EQ(result[2][0], (std::vector<Length>{4, 6, 7, 10, 11, 13, 17}));
    EXPECT_EQ(result[3][0], (std::vector<Length>{0, 4, 6, 7, 9, 10, 13}));
}

TEST(MeetInTheMiddle, MinimalMimPatternsWithRotationsIsSoundAndNeverLarger)
{
    // Item 0 is rotatable (widths 6 or 9); items 1-3 are fixed (4, 3, 7).
    // Pins down the multiple-choice computation; see
    // 'MinimalMimPatternsPreserveTwoDimensionalFeasibility' for its
    // soundness.
    std::vector<std::vector<Length>> item_widths = {{6, 9}, {4}, {3}, {7}};
    std::vector<std::vector<std::vector<Length>>> result = minimal_mim_patterns(item_widths, 20);
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0].size(), 2u);
    EXPECT_EQ(result[0][0], (std::vector<Length>{0, 3, 4, 7, 10, 11, 14}));  // rotation width 6
    EXPECT_EQ(result[0][1], (std::vector<Length>{0, 4, 7, 11}));             // rotation width 9
    EXPECT_EQ(result[1][0], (std::vector<Length>{0, 3, 6, 7, 9, 10, 13, 16}));
    EXPECT_EQ(result[2][0], (std::vector<Length>{10, 11, 13, 17}));
    EXPECT_EQ(result[3][0], (std::vector<Length>{0, 3, 4, 6, 7, 9, 10, 13}));
}

namespace
{

/** Item of a small two-dimensional instance: its (width, height) per rotation. */
using BruteForceItem = std::vector<std::pair<Length, Length>>;

/**
 * Check by brute force whether 'items' can be packed in a 'bin_width' x
 * 'bin_height' bin, each item (in rotation 'r') at an x-position of
 * 'x_positions[item_id][r]' and at a y-position which is a normal pattern of
 * the heights of the other items.
 */
bool brute_force_feasible(
        Length bin_width,
        Length bin_height,
        const std::vector<BruteForceItem>& items,
        const std::vector<std::vector<std::vector<Length>>>& x_positions)
{
    std::vector<std::vector<std::vector<Length>>> y_positions(items.size());
    for (size_t item_id = 0; item_id < items.size(); ++item_id) {
        std::vector<std::vector<Length>> other_heights;
        for (size_t other_item_id = 0; other_item_id < items.size(); ++other_item_id) {
            if (other_item_id == item_id)
                continue;
            std::vector<Length> heights;
            for (const auto& orientation: items[other_item_id])
                heights.push_back(orientation.second);
            other_heights.push_back(heights);
        }
        for (const auto& orientation: items[item_id]) {
            y_positions[item_id].push_back(normal_patterns(
                        other_heights,
                        bin_height - orientation.second));
        }
    }

    struct Placement { Length x; Length y; Length width; Length height; };
    std::vector<Placement> placements;
    std::function<bool(size_t)> search = [&](size_t item_id)
    {
        if (item_id == items.size())
            return true;
        for (size_t rotation_id = 0; rotation_id < items[item_id].size(); ++rotation_id) {
            Length width = items[item_id][rotation_id].first;
            Length height = items[item_id][rotation_id].second;
            for (Length x: x_positions[item_id][rotation_id]) {
                if (x < 0 || x + width > bin_width)
                    continue;
                for (Length y: y_positions[item_id][rotation_id]) {
                    bool overlap = false;
                    for (const Placement& placement: placements) {
                        if (x < placement.x + placement.width
                                && placement.x < x + width
                                && y < placement.y + placement.height
                                && placement.y < y + height) {
                            overlap = true;
                            break;
                        }
                    }
                    if (overlap)
                        continue;
                    placements.push_back({x, y, width, height});
                    if (search(item_id + 1))
                        return true;
                    placements.pop_back();
                }
            }
        }
        return false;
    };
    return search(0);
}

}

TEST(MeetInTheMiddle, MinimalMimPatternsPreserveTwoDimensionalFeasibility)
{
    // The minimal MIM patterns must preserve the feasibility of packing
    // problems: on random small two-dimensional instances, restricting the
    // x-positions of the items to them must not make a feasible instance
    // infeasible. The reference restricts them to the normal patterns.
    std::mt19937_64 generator(0);
    Counter number_of_feasible_instances = 0;
    for (Counter instance_id = 0; instance_id < 1000; ++instance_id) {
        Length bin_width = std::uniform_int_distribution<Length>(5, 10)(generator);
        Length bin_height = std::uniform_int_distribution<Length>(5, 10)(generator);
        size_t number_of_items = std::uniform_int_distribution<size_t>(3, 6)(generator);
        std::vector<BruteForceItem> items;
        Length area = 0;
        for (size_t item_id = 0; item_id < number_of_items; ++item_id) {
            Length width = std::uniform_int_distribution<Length>(1, bin_width)(generator);
            Length height = std::uniform_int_distribution<Length>(1, bin_height)(generator);
            BruteForceItem item = {{width, height}};
            if (width != height
                    && height <= bin_width
                    && width <= bin_height
                    && std::uniform_int_distribution<int>(0, 3)(generator) == 0) {
                item.push_back({height, width});
            }
            items.push_back(item);
            area += width * height;
        }
        if (area > bin_width * bin_height)
            continue;

        std::vector<std::vector<Length>> widths(items.size());
        for (size_t item_id = 0; item_id < items.size(); ++item_id)
            for (const auto& orientation: items[item_id])
                widths[item_id].push_back(orientation.first);
        std::vector<std::vector<std::vector<Length>>> normal(items.size());
        for (size_t item_id = 0; item_id < items.size(); ++item_id) {
            std::vector<std::vector<Length>> other_widths;
            for (size_t other_item_id = 0; other_item_id < items.size(); ++other_item_id)
                if (other_item_id != item_id)
                    other_widths.push_back(widths[other_item_id]);
            for (Length width: widths[item_id])
                normal[item_id].push_back(normal_patterns(other_widths, bin_width - width));
        }
        std::vector<std::vector<std::vector<Length>>> minimal
            = minimal_mim_patterns(widths, bin_width);

        bool feasible = brute_force_feasible(bin_width, bin_height, items, normal);
        if (!feasible)
            continue;
        number_of_feasible_instances++;
        std::stringstream ss;
        ss << "bin " << bin_width << "x" << bin_height << ", items";
        for (const BruteForceItem& item: items)
            ss << " " << item[0].first << "x" << item[0].second << ((item.size() > 1)? "r": "");
        EXPECT_TRUE(brute_force_feasible(bin_width, bin_height, items, minimal)) << ss.str();
    }
    EXPECT_GE(number_of_feasible_instances, 100);
}
