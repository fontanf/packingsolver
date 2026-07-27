#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "packingsolver/rectangle/reduction.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>

#include <map>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/** Rectangle (accounting for rotation) an item occupies. */
Rectangle item_footprint(const ItemType& item_type, bool rotate)
{
    return rotate? Rectangle{item_type.rect.y, item_type.rect.x}: item_type.rect;
}

/** Check that a solution has no two overlapping items and everything lies within its bin. */
void expect_geometrically_valid(const Solution& solution)
{
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const BinType& bin_type = solution.instance().bin_type(solution_bin.bin_type_id);
        for (size_t item_pos_1 = 0; item_pos_1 < solution_bin.items.size(); ++item_pos_1) {
            const SolutionItem& item_1 = solution_bin.items[item_pos_1];
            const ItemType& item_type_1 = solution.instance().item_type(item_1.item_type_id);
            Rectangle footprint_1 = item_footprint(item_type_1, item_1.rotate);
            EXPECT_LE(item_1.bl_corner.x + footprint_1.x, bin_type.rect.x);
            EXPECT_LE(item_1.bl_corner.y + footprint_1.y, bin_type.rect.y);
            for (size_t item_pos_2 = item_pos_1 + 1; item_pos_2 < solution_bin.items.size(); ++item_pos_2) {
                const SolutionItem& item_2 = solution_bin.items[item_pos_2];
                const ItemType& item_type_2 = solution.instance().item_type(item_2.item_type_id);
                Rectangle footprint_2 = item_footprint(item_type_2, item_2.rotate);
                EXPECT_FALSE(rect_intersection(
                        item_1.bl_corner, footprint_1,
                        item_2.bl_corner, footprint_2))
                    << "Items " << item_1.item_type_id << " and " << item_2.item_type_id << " overlap.";
            }
        }
    }
}

}

TEST(RectangleReduction, WideItemWithNarrowCompanionsExactFit)
{
    // Bin 10x10. Item 0 (8x10) is wide (8 > 10/2): its companion strip is
    // (2, 10). Items 1 (2x4) and 2 (2x6) exactly tile that strip stacked
    // vertically (4 + 6 = 10), so item 0 gets validated-wide-enlarged to
    // (10x10) with items 1/2 as its real companions - and since its
    // height (10) already equalled the bin's, that enlarged size exactly
    // matches the *true* bin too: 'reduce_full_bin_items' then claims it
    // as its own dedicated bin, capturing items 1/2 along with it (see
    // 'FullBinItem::companions_by_copy') rather than leaving item 0
    // present in the reduced instance.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(8, 10, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(2, 4, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(2, 6, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 3);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
}

TEST(RectangleReduction, TallItemWithNarrowCompanionsExactFit)
{
    // Bin 10x10. Item 0 (4x8) is tall (8 > 10/2) but not wide (4 <= 5):
    // its companion strip is (4, 2). Items 1 (1x2) and 2 (3x2) - neither
    // wide nor tall themselves - exactly tile that strip side by side
    // (1 + 3 = 4).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(4, 8, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(1, 2, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(3, 2, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 4);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 10);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    BinPos bin_pos = reduced_solution_builder.add_bin(0, 1);
    reduced_solution_builder.add_item(bin_pos, 0, Point{0, 0}, false);
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 3);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
}

TEST(RectangleReduction, WideAndTallItemWithFullBinCompanionsExactFit)
{
    // Bin 10x10. Item 0 (6x6) is both wide and tall: its companion "bin" is
    // the full (10, 10) bin, with item 0 fixed at the bottom-left corner.
    // Items 1 and 2 (4x5 each) together need the *entire* right-hand
    // column (4 wide, the full 10 tall) - more than item 0's own row (6
    // tall), so neither fits in the wide sub-case's smaller (4, 6) strip
    // check (which would need to hold both, 40 > 24 = strip area) or the
    // tall sub-case's (6, 4) strip check (both are 5 tall, taller than
    // that strip); only the "both" case's full-bin check has room for
    // them. This deliberately leaves the top-left (6, 4) corner
    // unpacked - the point is to exercise the full-bin/fixed-item
    // mechanism in isolation, not to reach a zero-waste packing. Item 0's
    // validated-both-enlargement sets both dimensions to the bin's own
    // (10x10), which then also exactly matches the *true* bin:
    // 'reduce_full_bin_items' claims it as its own dedicated bin,
    // capturing items 1/2 along with it (see
    // 'FullBinItem::companions_by_copy') rather than leaving item 0
    // present in the reduced instance.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(4, 5, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(4, 5, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 3);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
}

TEST(RectangleReduction, NarrowButTallItemPreventsWideReduction)
{
    // Bin 10x10. Item 0 (7x4) is wide (7 > 5): its companion strip is
    // (3, 4). Item 1 (2x8) is narrow enough to fall into that strip by
    // width alone (2 <= 3), but far too tall for it (8 > 4) - and does not
    // fit anywhere else in the instance either. A reduction that only
    // checked whether item 1 fits *this one* strip's height too (instead
    // of including every width-eligible item in the packing check
    // regardless of height, matching the width-only definition of the
    // candidate set) could wrongly conclude no companion applies and
    // enlarge item 0 anyway; item 0 must be left untouched.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(7, 4, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(2, 8, true);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 2);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 7);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 4);
}

TEST(RectangleReduction, PerfectPairSideBySideExactFit)
{
    // Bin 10x10. Items 0 and 1 (5x10 each) both span the bin's full height
    // and their widths sum exactly to the bin's full width (5 + 5 = 10),
    // but neither is "wide" under the wide/tall/both cases (2*5 = 10 is not
    // > 10): this is exactly the exact-boundary case those cases miss and
    // the perfect-pair rule catches. Both item types are removed entirely
    // (not enlarged): nothing else could ever share their dedicated bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 2);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, PerfectPairStackedExactFit)
{
    // Bin 10x8. Item 0 (10x3) and item 1 (10x5), both oriented, span the
    // bin's full width and their heights sum exactly to its full height
    // (3 + 5 = 8): the "horizontal split" case (unlike
    // 'PerfectPairSideBySideExactFit''s "vertical split").
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 8);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(10, 3, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(10, 5, true);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 2);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, BothGroupCatchesNonOrientedPairPerfectPairCannot)
{
    // Bin 10x8. Item 0 (10x3, oriented) spans the bin's full width. Item 1
    // (5x10, non-oriented) does not match stacked as declared (5 != 10),
    // and only its *rotated* footprint (10x5) would complete the pair
    // (3 + 5 = 8) - but 'reduce_perfect_pairs' requires both sides to be
    // 'oriented' (see its own doc comment for why: unlike
    // 'reduce_full_bin_items', matching only one bin dimension never
    // forces a non-oriented item's other orientation to become
    // infeasible, so nothing guarantees a true optimal solution couldn't
    // place it the other way), so 'reduce_perfect_pairs' itself never
    // finds this pair.
    //
    // 'reduce_both_groups' does, though: item 1 (2*10 > 10 and 2*5 > 8 when
    // rotated) is "both"-big via its rotated presentation alone (see
    // 'is_big''s own doc comment for why a non-oriented item is safe to
    // admit there, unlike for 'reduce_perfect_pairs'), and the actual
    // companion-bin feasibility check it runs - unlike
    // 'reduce_perfect_pairs''s own pure dimension match - directly proves
    // item 0 fits alongside item 1's rotated form, so the two are captured
    // together as a single dedicated "both" group instead.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 8);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(10, 3, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(5, 10, false);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();
    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, PerfectPairUnequalCopiesPartiallyReduces)
{
    // Bin 10x10. Items 0 and 1 (5x10 each) tile the bin exactly like in
    // 'PerfectPairSideBySideExactFit', but item 0 has 2 copies against
    // item 1's 4: only 'min(2,4)=2' copies can be paired into 2 dedicated
    // bins, fully consuming item 0 (removed entirely), while item 1 keeps
    // its leftover 2 copies as an ordinary item type in the reduced
    // instance ('reduce_perfect_pairs' never pairs an item type with
    // itself, so those 2 copies are left for the underlying solver -
    // which still finds, on its own, that they tile one more bin together
    // (5+5=10): 3 bins total, confirming the reduction stays sound
    // (optimal) even though it only handles part of the instance itself).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(0, 2);
    instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(1, 4);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).copies, 2);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 2);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 3);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 3);
}

TEST(RectangleReduction, PerfectPairLimitedByFiniteBinCopies)
{
    // Bin 10x10 with only 1 copy available (Feasibility objective). Items 0
    // and 1 (5x10 each, 2 copies each) would tile the bin exactly, twice
    // over (needing 2 dedicated bins), but only 1 bin copy exists in total:
    // the reduction must not reserve more dedicated bins than the bin type
    // actually has copies for, leaving both item types for the underlying
    // solver instead (which, with only 1 bin available for 2 copies of
    // each item, correctly proves the instance infeasible).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(0, 2);
    instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(1, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 0);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 2);
    EXPECT_EQ(reduction.instance().bin_type(0).copies, 1);
}

TEST(RectangleReduction, FullBinItemExactFit)
{
    // Bin 10x10. Item 0 (10x10, 2 copies) already exactly matches the
    // bin's own dimensions: nothing could ever share a bin with it, so it
    // is removed entirely and set aside as its own dedicated bin, one per
    // copy - even though it also satisfies the "both" case's 'is_big'
    // test (2*10 > 10 on both axes), 'try_reduce_group' would otherwise
    // leave it untouched (zero room for any companion to absorb).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_copies(0, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 2);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 2);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 2);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 2);
}

TEST(RectangleReduction, FullBinItemWithRotationExactFit)
{
    // Bin 10x8. Item 0 (8x10, non-oriented, 1 copy) does not match the
    // bin's dimensions as declared, but its rotated footprint (10x8)
    // does.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 8);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(8, 10, false);
    instance_builder.set_item_type_copies(0, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 1);
    EXPECT_TRUE(solution.bin(0).items[0].rotate);
    expect_geometrically_valid(solution);
}

TEST(RectangleReduction, FullBinItemExhaustsCapacityProvesInfeasible)
{
    // Bin 10x10 with only 1 copy available (Feasibility objective). Item 0
    // (10x10, 1 copy) exactly matches the bin and claims the sole bin copy
    // as its own dedicated bin; item 1 (2x2, 1 copy) is a real, unrelated
    // item still left over with nowhere to go - the reduction alone
    // already proves the instance infeasible, without needing to run any
    // solve on the (otherwise still-buildable) reduced instance.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(2, 2, true);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_TRUE(reduction.proven_infeasible());

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
}

TEST(RectangleReduction, CompanionlessEnlargementUpgradedToFullBinItem)
{
    // Bin 10x10. Item 0 (10x9, 1 copy) already spans the bin's full width
    // (a degenerate "wide" strip) and is "tall" (2*9 > 10) with a
    // genuinely checked, non-degenerate but empty companion strip (10
    // wide, 1 tall - there is no other item type in the instance to ever
    // fill it): the companionless step enlarges it to 10x10 with zero
    // companions. Once enlarged, it exactly matches the bin on both axes -
    // a strictly better reduction (full removal, as its own dedicated
    // bin) that 'reduce_full_bin_items' must be allowed to pick up in a
    // later round, even though the item is already 'enlarged'.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(10, 9, true);
    instance_builder.set_item_type_copies(0, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, BothCompanionlessCaptureUsesCorrectRotation)
{
    // Bin 10x8. Item 0 (7x9, non-oriented, 1 copy) is the only item type
    // in the instance. Its *declared* form (7x9) does not fit the bin at
    // all (9 > 8), only its *rotated* form (9x7) does - 'is_big''s own
    // 'Both' check does not itself distinguish this (it is a pure
    // threshold comparison, not a fit check: 2*7 > 10 and 2*9 > 8 already
    // hold for the declared form alone), so it is
    // 'reduce_companionless_items''s own responsibility to work out which
    // orientation the item can actually be captured at, exactly as
    // 'reduce_full_bin_items' already does - not simply assume the
    // declared one, which would silently build a geometrically invalid
    // dedicated bin (found via a regression: an earlier version of this
    // code always captured a companionless "both" item at its declared
    // orientation unconditionally).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 8);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(7, 9, false);
    instance_builder.set_item_type_copies(0, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();
    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    ASSERT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 1);
    EXPECT_TRUE(solution.bin(0).items[0].rotate);
    expect_geometrically_valid(solution);
}

TEST(RectangleReduction, FullBinItemViaShrunkBin)
{
    // Bin 10x10. Item 0 (9x9, oriented, 1 copy) is the only item type in
    // the instance: equation (7) ("shrinking the bins": see
    // 'compute_shrunk_bin_sizes') proves the bin's true achievable
    // width/height, given only this item, is exactly 9x9 - matching item
    // 0's own dimensions exactly - so nothing could ever share its bin
    // (the remaining (1,10)+(9,1) L-shaped margin is guaranteed
    // permanently unusable), even though item 0 does not physically fill
    // the true 10x10 bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(9, 9, true);
    instance_builder.set_item_type_copies(0, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, PerfectPairViaShrunkBin)
{
    // Bin 10x10. Items 0 and 1 (4x9 each, oriented, 1 copy each) do not
    // tile the true bin exactly (4+4=8 < 10, 9 < 10), but equation (7)
    // ("shrinking the bins": see 'compute_shrunk_bin_sizes') proves the
    // bin's true achievable width/height, given only these two items, is
    // exactly 8x9 - so the pair does exactly tile the *shrunk* bin, with
    // the remaining (10-8=2, 10-9=1) L-shaped margin guaranteed
    // permanently unusable by anything else (nothing else in the instance
    // is small enough to ever occupy it).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(4, 9, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(4, 9, true);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 2);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, PerfectPairCompanionHasItsOwnCompanions)
{
    // Bin 10x10. Item 1 (6x6, oriented) is "tall" (2*6 > 10): its
    // companion strip (6, 4) is exactly filled by item 2 (6x4), so item 1
    // gets validated-tall-enlarged to 6x10 with item 2 as its real
    // companion. Item 0 (4x10, oriented) already spans the bin's full
    // height and, together with item 1's *now*-enlarged width (6),
    // exactly tiles the bin (4+6=10): a 'PerfectPair' with item 0 as
    // survivor and item 1 as its companion - except item 1 itself already
    // carries a real companion (item 2) from its own earlier enlargement,
    // exercising the two-level nesting 'place_item_and_companions'
    // recurses through (item 0 -> item 1 -> item 2).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(4, 10, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(6, 4, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 3);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, BothGroupCompanionHasItsOwnCompanions)
{
    // Bin 10x10. Item 1 (3x7, oriented) is "tall" (2*7 > 10): its
    // companion strip (3, 3) is exactly filled by item 2 (3x3), so item 1
    // gets validated-tall-enlarged to 3x10 with item 2 as its real
    // companion - all within 'reduce_group(Tall)', which runs before
    // 'reduce_both_groups' in the same round.
    //
    // Item 0 (6x6, oriented) is "both"-big (2*6 > 10 on both axes). Its
    // companion-bin check offers every not-yet-removed item type as a
    // candidate - including item 1, even though item 1 is *already*
    // enlarged (an already-enlarged item type must not be excluded from
    // this candidate scan: excluding it would mean concluding "nothing
    // could fit here" using a narrower pool than the original problem
    // actually offers - see 'try_reduce_both_group''s own R-candidate
    // scan). Item 1's *current* dimensions (3x10) do fit alongside item 0
    // in the full 10x10 companion bin (6+3=9 <= 10), so the check
    // succeeds and item 0 absorbs item 1 as a real "both" companion -
    // exercising the two-level nesting 'place_item_and_companions'
    // recurses through (item 0 -> item 1 -> item 2), the same as
    // 'PerfectPairCompanionHasItsOwnCompanions' but reached via
    // 'reduce_both_groups' instead of 'reduce_perfect_pairs'.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(3, 7, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(3, 3, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 3);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}

TEST(RectangleReduction, TallCompanionlessEnlargementIndependentOfBlockedWideStrip)
{
    // Bin 10x10, all items oriented. Item 0 (6x9) is "big" under wide
    // (2*6 > 10), tall (2*9 > 10) AND both simultaneously. Its wide strip
    // (4x9, beside it) is blocked from companionless enlargement: item 2
    // (4x2) is narrow enough (width 4) to pass the necessary 'could_fit'
    // pre-filter for that strip, even though it (and nothing else here)
    // can actually exactly tile all 36 units of it - so wide's own
    // validated-companion search also fails to enlarge item 0, leaving it
    // genuinely undecided on that axis. Item 0's *tall* strip (6x1, above
    // it) is a completely different region: no item has height <= 1, so
    // it is provably, permanently empty regardless of the wide strip's
    // status. This is a regression test for a bug where companionless
    // enlargement's blocking check was wrongly shared across all three
    // axes via one flag threaded through wide/tall/both, so the wide
    // strip's real (if inconclusive) candidate silently vetoed the tall
    // strip's own, otherwise-conclusive, empty-margin proof - item 0 must
    // still get tall-companionless-enlarged to (6x10) independently of
    // the wide strip's outcome, at which point it exactly matches item 1
    // (4x10) as a 'PerfectPair' (6+4=10), leaving item 2 untouched in the
    // reduced instance.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(6, 9, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(4, 10, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(4, 2, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 4);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 2);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    BinPos bin_pos = reduced_solution_builder.add_bin(0, 1);
    reduced_solution_builder.add_item(bin_pos, 0, Point{0, 0}, false);
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    ASSERT_EQ(solution.number_of_bins(), 2);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 2);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 2);
}

TEST(RectangleReduction, WideThenTallRealCompanionsComposeIntoFullBinItem)
{
    // Bin 10x10, all items oriented. Item 0 (6x9) is "wide" (2*6 > 10):
    // its wide strip (4x9) is exactly tiled by item 1 (4x9), so
    // 'try_reduce_group' validated-wide-enlarges it to (10x9) with item 1
    // as a real companion - it does not yet span the bin's full height.
    // Item 0's *current* dimensions (10x9, already full width) are then
    // reconsidered for "tall": its now-full-width top strip (10x1) is
    // exactly tiled by item 2 (10x1), a real companion nothing else could
    // have offered before item 0's width grew. This requires
    // 'gather_sorted_big_items' to not exclude an already-enlarged item 0
    // from tall's own search, and 'try_reduce_group''s 'enlarge()' to
    // append item 2 onto item 0's 'companions_by_copy' alongside item 1
    // (rather than overwriting it), composing item 0 up to (10x10) - a
    // full bin, captured whole by 'reduce_full_bin_items' with both item 1
    // and item 2 as its direct (sibling, not nested) companions.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(6, 9, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(4, 9, true);
    instance_builder.set_item_type_copies(1, 1);
    instance_builder.add_item_type(10, 1, true);
    instance_builder.set_item_type_copies(2, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    ASSERT_EQ(solution.number_of_bins(), 1);
    ASSERT_EQ(solution.bin(0).items.size(), 3);
    expect_geometrically_valid(solution);

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_EQ(output.bin_packing_bound, 1);
}


// Reports the percentage of items removed by 'Reduction' across the full
// berkey1987/martello1998 benchmark (500 instances, classes 1-10, n in
// {20,40,60,80,100}, 10 instances each - the same benchmark and grouping as
// Côté, Haouari & Iori 2019/2021 Tables 4/5), for comparison against the
// paper's own '%rmv' column (average percentage of items removed by its
// Section 4.2 preprocessing). 'DISABLED_' since it prints a report rather
// than asserting anything and would otherwise slow down every routine
// 'ctest' run for no benefit - run explicitly with:
//   PackingSolver_rectangle_test --gtest_filter='*ReportBenchmarkRmv' --gtest_also_run_disabled_tests
// (from the repository root, so the 'data/rectangle/...' relative paths
// resolve).
TEST(RectangleReduction, DISABLED_ReportBenchmarkRmv)
{
    struct ClassInfo { std::string dir; int class_id; };
    std::vector<ClassInfo> classes = {
        {"berkey1987", 1}, {"berkey1987", 2}, {"berkey1987", 3},
        {"berkey1987", 4}, {"berkey1987", 5}, {"berkey1987", 6},
        {"martello1998", 7}, {"martello1998", 8}, {"martello1998", 9}, {"martello1998", 10},
    };
    std::vector<int> sizes = {20, 40, 60, 80, 100};

    std::map<int, std::vector<double>> by_class;
    std::map<int, std::vector<double>> by_size;
    std::vector<double> all;

    for (const ClassInfo& class_info: classes) {
        for (int size: sizes) {
            for (int instance_num = 1; instance_num <= 10; ++instance_num) {
                char class_str[8];
                snprintf(class_str, sizeof(class_str), "%02d", class_info.class_id);
                std::ostringstream base;
                base << "data/rectangle/" << class_info.dir << "/Class_" << class_str
                     << ".2bp_" << size << "_" << instance_num;
                InstanceBuilder instance_builder;
                instance_builder.set_objective(Objective::BinPacking);
                instance_builder.read_item_types(base.str() + "_items.csv");
                instance_builder.read_bin_types(base.str() + "_bins.csv");
                instance_builder.set_bin_type_copies(0, -1);
                instance_builder.set_item_types_oriented();
                Instance instance = instance_builder.build();

                ItemPos total_items = instance.number_of_items();
                Reduction reduction(instance);
                ItemPos remaining_items = reduction.instance().number_of_items();
                double pct = 100.0 * (total_items - remaining_items) / total_items;

                by_class[class_info.class_id].push_back(pct);
                by_size[size].push_back(pct);
                all.push_back(pct);
            }
        }
    }

    auto avg = [](const std::vector<double>& values)
        {
            double sum = 0.0;
            for (double value: values)
                sum += value;
            return sum / values.size();
        };

    std::cout << "Per class:" << std::endl;
    for (const ClassInfo& class_info: classes)
        std::cout << "  class " << class_info.class_id << ": "
            << avg(by_class[class_info.class_id]) << "%" << std::endl;

    std::cout << "Per size:" << std::endl;
    for (int size: sizes)
        std::cout << "  n=" << size << ": " << avg(by_size[size]) << "%" << std::endl;

    std::cout << "Overall: " << avg(all) << "%" << std::endl;
}

TEST(RectangleReduction, ShrunkBinViaMultipleCopiesOfSameItem)
{
    // Bin 20x10 (asymmetric so only the height axis is ever "big",
    // keeping this isolated to a single case). Item 0 (5x6, oriented, 1
    // copy) is tall (2*6=12 > 10): its own companion strip is (5,
    // 10-target_bin_h). Item 1 (5x1, oriented, 3 copies) exactly tiles a
    // (5,3) strip stacked three high (1+1+1=3), which is exactly what
    // equation (7) ("shrinking the bins") computes as the tallest
    // achievable combination not exceeding the bin's true height: item
    // 0's own height (6) plus all three separate copies of item 1's
    // height (1 each) sum to exactly 9 (<= 10; a fourth unit would
    // overshoot). This specifically requires each of the 3 copies to be
    // counted as its own individual candidate in the underlying subset-sum
    // (not e.g. item 1's type counted only once regardless of its copies)
    // - the target height (9) is unreachable from any smaller number of
    // copies. Item 0 gets validated-tall-enlarged to 5x9 with all three
    // copies of item 1 as its real companions, rather than to the bin's
    // true height (10).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(20, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(5, 6, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(5, 1, true);
    instance_builder.set_item_type_copies(1, 3);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 5);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 9);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 0);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    BinPos bin_pos = reduced_solution_builder.add_bin(0, 1);
    reduced_solution_builder.add_item(bin_pos, 0, Point{0, 0}, false);
    Solution reduced_solution = reduced_solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    ASSERT_EQ(solution.bin(0).items.size(), 4);
    expect_geometrically_valid(solution);
}

TEST(RectangleReduction, ShrunkBinViaNonOrientedItemRotation)
{
    // Bin 10x10. Item 1 (5x9, non-oriented, 1 copy) contributes to
    // equation (7)'s width computation via *either* of its orientations:
    // declared width 5, or rotated width 9 - and only the rotated value
    // (9 <= 10) is what the computed shrunk width of 9 actually comes
    // from (item 0's own width (6) alone, or item 1's declared width (5)
    // alone, never reach it; item 0 + item 1's declared width (6+5=11)
    // exceeds the true bin width (10) so isn't achievable at all),
    // proving the multiple-choice subset-sum genuinely considers rotation
    // (unlike a plain per-item subset sum, which would need to commit to
    // one fixed value per item upfront and could never reach 9 here). The
    // same reasoning shrinks the height to 9 too (item 1's *declared*
    // height, 9, dominates there instead - item 0's own height (3) is too
    // small to ever combine with it under the true bin height of 10).
    //
    // With 'is_big''s 'Both' case now admitting a non-oriented item (see
    // its own doc comment), item 1's rotated presentation (2*9 > 9 and
    // 2*5 > 9, against the *shrunk* 9x9 companion bin) makes it eligible
    // there, and the companion-bin check - run against that same shrunk
    // 9x9 bin, not the true 10x10 one - finds item 0 fits alongside it
    // (stacked: item 0's height 3 plus item 1's rotated height 5 sums to
    // exactly 9). Both items end up captured together as a single
    // dedicated "both" group - a stronger result than the plain
    // wide-companionless enlargement this test originally demonstrated,
    // but one that still only succeeds because of the same rotation-aware
    // shrunk bin computation: at the *true* 10x10 bin size this pairing
    // would prove nothing distinctive, since almost anything fits there.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(6, 3, true);
    instance_builder.set_item_type_copies(0, 1);
    instance_builder.add_item_type(5, 9, false);
    instance_builder.set_item_type_copies(1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 1);

    SolutionBuilder reduced_solution_builder(reduction.instance());
    Solution reduced_solution = reduced_solution_builder.build();
    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), 1);
    expect_geometrically_valid(solution);
}

TEST(RectangleReduction, NoReductionWhenBinHasDefects)
{
    // Bin 10x10 with a defect: item 0 (10x10, 2 copies) would otherwise be
    // captured as two dedicated bins (see 'FullBinItemExactFit'), but the
    // companion-bin checks this class performs are always plain,
    // defect-free rectangles - a defect could sit exactly where a
    // companion (or the big item itself) needs to go, invisibly to those
    // checks - so the whole reduction is skipped: 'instance()' stays an
    // unchanged copy of the original instance.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_defect(bin_type_id, 0, 0, 1, 1);
    instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_copies(0, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 10);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 10);
    EXPECT_EQ(reduction.instance().item_type(0).copies, 2);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 0);
}

TEST(RectangleReduction, NoReductionWhenUnloadingConstraintSet)
{
    // Same instance as 'FullBinItemExactFit', but with a non-'None'
    // unloading constraint set: an unloading order that holds for the big
    // item and its companions checked in isolation says nothing about
    // whether it still holds once other items - chosen later by the
    // downstream solve, never part of that isolated check - join the same
    // bin, so the whole reduction is skipped.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.set_unloading_constraint(UnloadingConstraint::OnlyXMovements);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_copies(0, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 10);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 10);
    EXPECT_EQ(reduction.instance().item_type(0).copies, 2);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 0);
}

TEST(RectangleReduction, NoReductionWhenBinWeightConstrained)
{
    // Same instance as 'FullBinItemExactFit', but with a finite bin
    // weight capacity and a non-zero item weight: total bin weight is a
    // whole-bin aggregate over every item sharing it, the same
    // whole-bin argument as the unloading constraint case, so the whole
    // reduction is skipped.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.set_bin_type_maximum_weight(bin_type_id, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.set_item_type_weight(item_type_id, 5);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    ASSERT_EQ(reduction.instance().number_of_item_types(), 1);
    EXPECT_EQ(reduction.instance().item_type(0).rect.x, 10);
    EXPECT_EQ(reduction.instance().item_type(0).rect.y, 10);
    EXPECT_EQ(reduction.instance().item_type(0).copies, 2);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 0);
}

TEST(RectangleReduction, ReductionStillAppliesWithFiniteWeightButZeroItemWeights)
{
    // Same instance as 'NoReductionWhenBinWeightConstrained', but every
    // item weight is left at its default (0): a finite bin weight
    // capacity alone can never be exceeded by items that all weigh
    // nothing, so this is not the risky case the previous test guards
    // against, and the reduction must still apply normally.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    instance_builder.set_bin_type_maximum_weight(bin_type_id, 100);
    instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_copies(0, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    EXPECT_EQ(reduction.instance().number_of_item_types(), 0);
    EXPECT_EQ(reduction.number_of_dedicated_bins(), 2);
}
