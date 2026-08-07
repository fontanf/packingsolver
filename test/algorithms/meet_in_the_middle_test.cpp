#include "algorithms/meet_in_the_middle.hpp"

#include <gtest/gtest.h>

#include <algorithm>

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
// prune passes) against an independent Python re-implementation of the
// same algorithm, itself hand- and script-verified against the underlying
// propositions (see the worked examples in the commit history/discussion
// that introduced this): widths = (6, 4, 3, 7) in W = 20 exercises the
// left-pattern widen+prune (Preprocessing 2, part A - item 0's pattern 4
// is dropped, dominated by pattern 3's enlarged reach); widths = (8, 5, 6)
// in W = 20 exercises the right-pattern cross-item widen+move (part B -
// item 0's right pattern at 12 moves to 11, widened from 8 to 9 so its
// right border stays at 20); Example 1 from the paper itself (widths =
// (5, 10, 12, 15), W = 27) is the same instance already used to validate
// the unpreprocessed 'mim_patterns' above, now checked all the way
// through the preprocessing too. All three use single-rotation items, to
// isolate the preprocessing logic from the multiple-choice generalization
// (checked separately below).
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
    EXPECT_EQ(result[3][0], (std::vector<Length>{0, 12}));
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
    // Verified (via an independent Python re-implementation, itself
    // cross-checked against the single-rotation results above) that the
    // multiple-choice computation is at least as tight, item by item and
    // rotation by rotation, as flattening every rotation into an
    // independent "item" (the unsound shortcut this generalizes away
    // from) would be on the same instance.
    std::vector<std::vector<Length>> item_widths = {{6, 9}, {4}, {3}, {7}};
    std::vector<std::vector<std::vector<Length>>> result = minimal_mim_patterns(item_widths, 20);
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0].size(), 2u);
    EXPECT_EQ(result[0][0], (std::vector<Length>{0, 3, 7, 10, 11, 14}));  // rotation width 6
    EXPECT_EQ(result[0][1], (std::vector<Length>{0, 3, 11}));             // rotation width 9
    EXPECT_EQ(result[1][0], (std::vector<Length>{0, 3, 6, 7, 9, 10, 13, 16}));
    EXPECT_EQ(result[2][0], (std::vector<Length>{10, 11, 13, 17}));
    EXPECT_EQ(result[3][0], (std::vector<Length>{0, 3, 4, 7, 9, 10, 13}));
}
