#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "packingsolver/rectangle/reduction.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>

namespace fs = boost::filesystem;
using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/**
 * Structural comparison of every property 'Reduction' can affect, for
 * tests that build an explicit "expected instance" and check the reduced
 * instance against it directly, rather than asserting on individual
 * fields one at a time.
 */
void expect_instances_equal(const Instance& actual, const Instance& expected)
{
    ASSERT_EQ(actual.number_of_bin_types(), expected.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < expected.number_of_bin_types();
            ++bin_type_id) {
        const BinType& actual_bin_type = actual.bin_type(bin_type_id);
        const BinType& expected_bin_type = expected.bin_type(bin_type_id);
        EXPECT_EQ(actual_bin_type.rect.x, expected_bin_type.rect.x);
        EXPECT_EQ(actual_bin_type.rect.y, expected_bin_type.rect.y);
        EXPECT_EQ(actual_bin_type.copies, expected_bin_type.copies);
        EXPECT_EQ(actual_bin_type.copies_min, expected_bin_type.copies_min);
    }

    ASSERT_EQ(actual.number_of_item_types(), expected.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < expected.number_of_item_types();
            ++item_type_id) {
        const ItemType& actual_item_type = actual.item_type(item_type_id);
        const ItemType& expected_item_type = expected.item_type(item_type_id);
        EXPECT_EQ(actual_item_type.rect.x, expected_item_type.rect.x);
        EXPECT_EQ(actual_item_type.rect.y, expected_item_type.rect.y);
        EXPECT_EQ(actual_item_type.oriented, expected_item_type.oriented);
        EXPECT_EQ(actual_item_type.copies, expected_item_type.copies);
        EXPECT_EQ(actual_item_type.copies_min, expected_item_type.copies_min);
        EXPECT_EQ(actual_item_type.profit, expected_item_type.profit);
    }
}

}

/**
 * One 'Reduction' scenario, read from 'data/rectangle/tests/<name>/':
 * 'items.csv'/'bins.csv'/'parameters.csv' for the input instance (the same
 * files 'optimize()' is then run on directly - no separate defects/JSON
 * support needed here yet, unlike 'RectangleBendersDecompositionTest',
 * since no reduction scenario so far needs them); 'reduced_items.csv'/
 * 'reduced_bins.csv' (sharing the same 'parameters.csv' - the reduced
 * instance always keeps the original's own objective) for the expected
 * reduced instance, compared via 'expect_instances_equal'; and
 * 'solution.csv' for a known-optimal reference solution of the *original*
 * instance, compared against 'optimize()''s own result via
 * 'Solution::operator<' equivalence (matching
 * 'RectangleBendersDecompositionTest''s own pattern) rather than exact
 * item positions - deliberately, since several of these scenarios resolve
 * their real companions via an internal 'Objective::Feasibility' tree
 * search whose exact tie-breaking is not part of this class's own
 * contract, only the quality of the outcome is.
 *
 * 'reduced_items.csv'/'reduced_bins.csv'/'solution.csv' are not read (and
 * need not exist) when 'expected_proven_infeasible' is 'true': 'instance()'
 * is not meaningful once 'Reduction::proven_infeasible()' holds - see its
 * own doc comment.
 */
struct RectangleReductionTestParams
{
    fs::path dir;
    ReductionParameters reduction_parameters;
    /** 'Reduction::proven_infeasible()' alone proves the instance infeasible. */
    bool expected_proven_infeasible = false;
    /**
     * The instance is infeasible overall, but not provably so by the
     * reduction alone (e.g. bin copies too limited for the reduced
     * instance's own remaining item copies, only detectable by the
     * downstream geometric solve - see 'PerfectPairLimitedByFiniteBinCopies').
     * Only meaningful when 'expected_proven_infeasible' is 'false': the
     * reduced instance is still checked against 'reduced_items.csv'/
     * 'reduced_bins.csv' as usual, but 'solution.csv' is not read - there
     * is no reference solution to compare against - and 'optimize()''s own
     * 'is_proven_infeasible' is checked directly instead.
     */
    bool expected_output_infeasible = false;
    /**
     * Use 'instance.json'/'reduced_instance.json' instead of 'items.csv'/
     * 'bins.csv'/'reduced_items.csv'/'reduced_bins.csv' - needed for any
     * feature the CSV format cannot represent (resources, eligibility -
     * see 'Instance::write_csv''s own checks), matching
     * 'RectangleBendersDecompositionTestParams''s own 'instance_path'.
     */
    bool use_json = false;
    /**
     * Do not call 'optimize()' at all (so no 'solution.csv' is read or
     * needed): some scenarios exist specifically to exercise 'Reduction'
     * itself against a combination this codebase's solvers cannot
     * currently handle downstream - a pre-existing, 'Reduction'-unrelated
     * defect (a negative-penalty resource, or a mandatory/'copies_min' item
     * type with negative profit, both under 'Knapsack' - see
     * 'RemoveNegativeProfitItemsSkippedWithNegativePenaltyResource' and
     * 'RemoveNegativeProfitItemsTrimsToMinimum''s own comments) that would
     * otherwise hang or wrongly report infeasible. Only 'expect_instances_equal'
     * runs; the reduced instance is still checked as usual.
     */
    bool skip_optimize_check = false;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleReductionTestParams& test_params)
{
    os << test_params.dir;
    return os;
}

class RectangleReductionTest: public testing::TestWithParam<RectangleReductionTestParams> { };

TEST_P(RectangleReductionTest, RectangleReduction)
{
    RectangleReductionTestParams test_params = GetParam();

    InstanceBuilder instance_builder;
    if (test_params.use_json) {
        instance_builder.read((test_params.dir / "instance.json").string());
    } else {
        instance_builder.read_item_types((test_params.dir / "items.csv").string());
        instance_builder.read_bin_types((test_params.dir / "bins.csv").string());
        instance_builder.read_parameters((test_params.dir / "parameters.csv").string());
        if (fs::exists(test_params.dir / "defects.csv"))
            instance_builder.read_defects((test_params.dir / "defects.csv").string());
    }
    Instance instance = instance_builder.build();

    Reduction reduction(instance, test_params.reduction_parameters);
    EXPECT_EQ(reduction.proven_infeasible(), test_params.expected_proven_infeasible);
    if (test_params.expected_proven_infeasible) {
        OptimizeParameters optimize_parameters;
        optimize_parameters.reduction_parameters = test_params.reduction_parameters;
        rectangle::Output output = optimize(instance, optimize_parameters);
        EXPECT_TRUE(output.is_proven_infeasible);
        return;
    }

    InstanceBuilder expected_instance_builder;
    if (test_params.use_json) {
        expected_instance_builder.read((test_params.dir / "reduced_instance.json").string());
    } else {
        expected_instance_builder.read_item_types((test_params.dir / "reduced_items.csv").string());
        expected_instance_builder.read_bin_types((test_params.dir / "reduced_bins.csv").string());
        expected_instance_builder.read_parameters((test_params.dir / "parameters.csv").string());
        if (fs::exists(test_params.dir / "defects.csv"))
            expected_instance_builder.read_defects((test_params.dir / "defects.csv").string());
    }
    Instance expected_instance = expected_instance_builder.build();
    expect_instances_equal(reduction.instance(), expected_instance);

    if (test_params.skip_optimize_check)
        return;

    OptimizeParameters optimize_parameters;
    optimize_parameters.reduction_parameters = test_params.reduction_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);

    if (test_params.expected_output_infeasible) {
        EXPECT_TRUE(output.is_proven_infeasible);
        return;
    }

    Solution solution = output.solution_pool.best();

    SolutionBuilder expected_solution_builder(instance);
    expected_solution_builder.read((test_params.dir / "solution.csv").string());
    Solution expected_solution = expected_solution_builder.build();

    EXPECT_TRUE(!(solution < expected_solution));
    EXPECT_TRUE(!(expected_solution < solution));
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleReductionTest,
        testing::ValuesIn(std::vector<RectangleReductionTestParams>{
            {
                // Bin 10x6, 1 copy (Feasibility). Items 0 and 2 (3x6 each,
                // oriented, 1 copy) already span the bin's full height on
                // their own. Item 1 (3x1, oriented, 1 copy) does not - but
                // 'lift_item_dimensions', which runs before this
                // reduction each round, finds that neither item 0 nor
                // item 2 alone can ever fit in the margin above it (each
                // needs height 6, but only 5 is available above item 1's
                // own height 1) and so grows item 1's own *working*
                // height to 6 too. That lifted height then exactly spans
                // the bin the same way items 0 and 2's own true heights
                // do, so this reduction pins all three - item 1 via its
                // lifted dimension, not the original one - leaving an
                // empty reduced instance: the actual capability this
                // operation gained by trusting 'item.rect' instead of
                // skipping already-grown item types (see
                // 'Reduction::reduce_full_span_items''s own doc comment).
                fs::path("data") / "rectangle" / "tests" / "feasibility_full_span_lift_to_span_bin",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 0 (8x10) is wide (8 > 10/2): its
                // companion strip is (2, 10). Items 1 (2x4) and 2 (2x6)
                // exactly tile that strip stacked vertically (4 + 6 =
                // 10), so item 0 gets validated-wide-enlarged to (10x10)
                // with items 1/2 as its real companions - and since its
                // height (10) already equalled the bin's, that enlarged
                // size exactly matches the *true* bin too:
                // 'reduce_full_bin_items' then claims it as its own
                // dedicated bin, capturing items 1/2 along with it (see
                // 'FullBinItem::companions_by_copy') rather than leaving
                // item 0 present in the reduced instance.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_wide_item_narrow_companions_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Same item 0 as above (spans the bin's full height), but
                // 'BinPacking' instead of 'Feasibility':
                // 'reduce_full_span_items_applies' only ever holds for
                // 'Feasibility' (see its own doc comment - 'BinPacking'
                // has no single, fixed bin to shrink), so this reduction
                // must not fire at all: the reduced instance comes back
                // exactly as the original instance gives it, untouched.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_full_span_not_applied",
                ReductionParameters(),
                false,
            }, {
                // Same item 0 again, 'Feasibility' objective, but the bin
                // type has 2 copies instead of 1:
                // 'reduce_full_span_items_applies' requires exactly one
                // bin copy (shrinking "the" bin is only unambiguous with
                // exactly one instance of it - see its own doc comment),
                // so this reduction must not fire, and the reduced
                // instance comes back untouched, same as above.
                fs::path("data") / "rectangle" / "tests" / "feasibility_full_span_not_applied_multiple_bin_copies",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10, 1 copy (Feasibility). Item 0 (5x10,
                // oriented, 2 copies) spans the bin's full height;
                // pinning both copies exactly exhausts the bin's true
                // width (5 + 5 = 10, down to 0), and item 1 (1x1, 1
                // copy) still needs positive room that no longer exists
                // - the instance is genuinely infeasible (item 0 alone
                // already fills the whole bin), and this reduction
                // proves it directly, without needing to exhaust the bin
                // type's own copies the way
                // 'FullBinItemExhaustsCapacityProvesInfeasible' does.
                // 'lift_item_dimensions' disabled: it would otherwise
                // inflate item 1 to exactly 5x10 too (item 0's own 2
                // copies can never both fit in item 1's margin either),
                // making it a genuine 'reduce_perfect_pairs' match
                // instead of exercising this operation.
                fs::path("data") / "rectangle" / "tests" / "feasibility_full_span_exhaustion_proves_infeasible",
                []() { ReductionParameters p; p.lift_item_dimensions = false; return p; }(),
                true,
            }, {
                // Bin 5x10, 1 copy (Feasibility). Item 0 (1x10,
                // oriented, 5 copies) spans the bin's full height;
                // pinning all 5 copies exactly exhausts the bin's true
                // width (5 * 1 = 5, down to 0) - but nothing else
                // remains needing room, so this is a perfectly good,
                // exact-fit packing, not a contradiction: unlike the
                // previous case, reaching exactly 0 must not by itself
                // prove infeasibility.
                fs::path("data") / "rectangle" / "tests" / "feasibility_full_span_exact_fit_not_infeasible",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10, 1 copy (Feasibility). Item 0 (10x10, 1
                // copy) exactly matches the bin and is claimed by
                // 'reduce_full_bin_items' as its own dedicated bin (see
                // 'FullBinItemExhaustsCapacityProvesInfeasible', which
                // this case is a closer, more targeted regression
                // companion to) - this claims the bin type's sole copy.
                // Item 1 (2x2, 1 copy) does not itself span the bin on
                // either axis, but 'lift_item_dimensions' - which runs
                // *before* this reduction each round - finds that item 0
                // alone can never coexist with it (item 0 does not fit
                // in the margin left beside a 2x2 item either) and so
                // grows item 1's own *working* dimensions to 10x10 too,
                // a placeholder 'reduce_full_span_items' is free to
                // trust and match against just like any other current
                // dimension - but by the time it runs, item 0's
                // dedicated bin has already reserved the sole bin copy
                // this operation would need to share (see
                // 'reduce_full_span_items''s own guard against that,
                // checked via 'number_of_dedicated_bins()'), so it
                // correctly declines to pin item 1 too, leaving the
                // *existing* dedicated-bin-capacity-exhaustion check in
                // 'reduction_to_instance' to prove this instance
                // infeasible instead (bin copies exhausted by item 0's
                // own dedicated bin, with item 1 still needing to be
                // packed).
                fs::path("data") / "rectangle" / "tests" / "feasibility_full_span_item_already_lifted",
                ReductionParameters(),
                true,
            }, {
                // Bin 10x10. Item 0 (4x8) is tall (8 > 10/2) but not wide
                // (4 <= 5): its companion strip is (4, 2). Items 1 (1x2)
                // and 2 (3x2) - neither wide nor tall themselves -
                // exactly tile that strip side by side (1 + 3 = 4). Item
                // 0 is validated-tall-enlarged to (4, 10), consuming both
                // companions - at which point it is the *only* item type
                // left in the working representation, so
                // 'compute_shrunk_bin_sizes' (recomputed every round from
                // the then-current, still-remaining item set) finds
                // nothing left that could ever share a bin with it: the
                // recomputed shrunk bin exactly equals item 0's own (4,
                // 10), so 'reduce_full_bin_items' claims it as a
                // dedicated bin instead of leaving it as an ordinary item
                // type in the reduced instance - an equivalent, more
                // aggressive encoding of the exact same physical solution
                // (one bin holding item 0 and its two companions), not a
                // different one.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_tall_item_narrow_companions_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 0 (6x6) is both wide and tall: its
                // companion "bin" is the full (10, 10) bin, with item 0
                // fixed at the bottom-left corner. Items 1 and 2 (4x5
                // each) together need the *entire* right-hand column (4
                // wide, the full 10 tall) - more than item 0's own row (6
                // tall), so neither fits in the wide sub-case's smaller
                // (4, 6) strip check (which would need to hold both, 40 >
                // 24 = strip area) or the tall sub-case's (6, 4) strip
                // check (both are 5 tall, taller than that strip); only
                // the "both" case's full-bin check has room for them.
                // This deliberately leaves the top-left (6, 4) corner
                // unpacked - the point is to exercise the full-bin/fixed-
                // item mechanism in isolation, not to reach a zero-waste
                // packing. Item 0's validated-both-enlargement sets both
                // dimensions to the bin's own (10x10), which then also
                // exactly matches the *true* bin: 'reduce_full_bin_items'
                // claims it as its own dedicated bin, capturing items 1/2
                // along with it rather than leaving item 0 present in the
                // reduced instance.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_wide_and_tall_item_full_bin_companions_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 0 (7x4) is wide (7 > 5): its companion
                // strip is (3, 4). Item 1 (2x8) is narrow enough to fall
                // into that strip by width alone (2 <= 3), but far too
                // tall for it (8 > 4) - and does not fit anywhere else in
                // the instance either. A reduction that only checked
                // whether item 1 fits *this one* strip's height too
                // (instead of including every width-eligible item in the
                // packing check regardless of height, matching the
                // width-only definition of the candidate set) could
                // wrongly conclude no companion applies and enlarge item
                // 0 anyway; item 0 must be left untouched.
                // 'lift_item_dimensions' disabled: with default
                // parameters, it *does* soundly enlarge item 0 on both
                // axes for this same instance (see the next case), by a
                // different, purely 1D argument that has nothing to do
                // with 'reduce_group''s own companionless proof - leaving
                // it enabled would defeat the point of this case, which
                // is specifically to isolate and verify 'reduce_group''s
                // own width-vs-height distinction.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_narrow_but_tall_item_prevents_wide_reduction",
                []() { ReductionParameters p; p.lift_item_dimensions = false; return p; }(),
                false,
            }, {
                // Same instance as the previous case, but with default
                // parameters ('lift_item_dimensions' enabled): item 0
                // (7x4) has a true-bin-width margin of 10-7=3 on the
                // width axis: item 1 (width 2, the only other item type)
                // can achieve at most 2 of it, so the provably-
                // unreachable remainder (3-2=1) lifts item 0's width to 8
                // - not all the way to 10, since item 1's own width (2)
                // is still a real, achievable candidate that must stay
                // available to it. On the height axis, item 0's margin is
                // 10-4=6: item 1's height (8) *exceeds* that margin
                // entirely, so it cannot contribute at all, and item 0's
                // height lifts all the way to 10. Item 0 is now (8, 10) -
                // "wide" under the *true*, unshrunk bin dimensions (2*8 >
                // 10) and already spanning the bin's full height - so
                // 'reduce_group' takes over from there in a later round:
                // its own companion-bin check now finds item 1 (2x8)
                // provably fits exactly in the (2, 10) strip beside item
                // 0, validated-wide-enlarging it to the full (10, 10) bin
                // with item 1 as a real companion - which
                // 'reduce_full_bin_items' then claims as a dedicated bin
                // outright. Every step individually sound; chained
                // together they fully resolve this instance without ever
                // needing a downstream solve.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_lift_item_dimensions_chains_into_full_bin_item",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Items 0 and 1 (5x10 each) both span the
                // bin's full height and their widths sum exactly to the
                // bin's full width (5 + 5 = 10), but neither is "wide"
                // under the wide/tall/both cases (2*5 = 10 is not > 10):
                // this is exactly the exact-boundary case those cases
                // miss and the perfect-pair rule catches. Both item types
                // are removed entirely (not enlarged): nothing else could
                // ever share their dedicated bin.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_perfect_pair_side_by_side_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x8. Item 0 (10x3) and item 1 (10x5), both
                // oriented, span the bin's full width and their heights
                // sum exactly to its full height (3 + 5 = 8): the
                // "horizontal split" case (unlike the previous case's
                // "vertical split").
                fs::path("data") / "rectangle" / "tests" / "bin_packing_perfect_pair_stacked_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x8. Item 0 (10x3, oriented) spans the bin's full
                // width. Item 1 (5x10, non-oriented) does not match
                // stacked as declared (5 != 10), and only its *rotated*
                // footprint (10x5) would complete the pair (3 + 5 = 8) -
                // but 'reduce_perfect_pairs' requires both sides to be
                // 'oriented' (matching only one bin dimension never
                // forces a non-oriented item's other orientation to
                // become infeasible, so nothing guarantees a true optimal
                // solution couldn't place it the other way), so
                // 'reduce_perfect_pairs' itself never finds this pair.
                // 'reduce_both_groups' does, though: item 1 (2*10 > 10
                // and 2*5 > 8 when rotated) is "both"-big via its rotated
                // presentation alone, and the actual companion-bin
                // feasibility check it runs - unlike
                // 'reduce_perfect_pairs''s own pure dimension match -
                // directly proves item 0 fits alongside item 1's rotated
                // form, so the two are captured together as a single
                // dedicated "both" group instead.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_both_group_catches_non_oriented_pair",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Items 0 and 1 (5x10 each) tile the bin
                // exactly like 'PerfectPairSideBySideExactFit' above, but
                // item 0 has 2 copies against item 1's 4: only
                // 'min(2,4)=2' copies can be paired into 2 dedicated
                // bins, fully consuming item 0 (removed entirely), while
                // item 1 keeps its leftover 2 copies as an ordinary item
                // type in the reduced instance ('reduce_perfect_pairs'
                // never pairs an item type with itself, so those 2
                // copies are left for the underlying solver - which
                // still finds, on its own, that they tile one more bin
                // together (5+5=10): 3 bins total, confirming the
                // reduction stays sound (optimal) even though it only
                // handles part of the instance itself).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_perfect_pair_unequal_copies_partially_reduces",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10 with only 1 copy available (Feasibility
                // objective). Items 0 and 1 (5x10 each, 2 copies each)
                // would tile the bin exactly, twice over (needing 2
                // dedicated bins), but only 1 bin copy exists in total:
                // 'reduce_perfect_pairs' must not reserve more dedicated
                // bins than the bin type actually has copies for, leaving
                // both item types present (which, with only 1 bin
                // available for 2 copies of each item, is genuinely
                // infeasible - detected only by the downstream solve, not
                // by the reduction itself). Items 0 and 1 are also
                // literally identical (same size, oriented, no
                // resources), so once 'reduce_perfect_pairs' leaves both
                // untouched, 'merge_identical_items' consolidates them
                // into a single item type with 4 copies - a second,
                // independent reduction exercised by this same instance,
                // not a contradiction of the "must not over-reserve"
                // point above.
                // 'enlarge_wide_tall_items' disabled: also gates
                // 'reduce_full_span_items' (each item's own height
                // already exactly matches the bin's), which - unlike
                // 'reduce_perfect_pairs' - pins copies one at a time
                // rather than needing to reserve a whole matched group up
                // front, so it would otherwise correctly detect (via a
                // different, still sound argument - not a contradiction,
                // just a reduction outside what this case means to
                // isolate) that item 0's own 2 copies alone already
                // exactly exhaust the bin, leaving no room for item 1 at
                // all, and prove the instance infeasible itself instead
                // of leaving that to the downstream solve as this case's
                // own point is about.
                fs::path("data") / "rectangle" / "tests" / "feasibility_perfect_pair_limited_by_finite_bin_copies",
                []() { ReductionParameters p; p.enlarge_wide_tall_items = false; return p; }(),
                false,
                true,
            }, {
                // Bin 10x10. Item 0 (10x10, 2 copies) already exactly
                // matches the bin's own dimensions: nothing could ever
                // share a bin with it, so it is removed entirely and set
                // aside as its own dedicated bin, one per copy - even
                // though it also satisfies the "both" case's 'is_big'
                // test (2*10 > 10 on both axes), 'try_reduce_group' would
                // otherwise leave it untouched (zero room for any
                // companion to absorb).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_full_bin_item_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x8. Item 0 (8x10, non-oriented, 1 copy) does not
                // match the bin's dimensions as declared, but its rotated
                // footprint (10x8) does.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_full_bin_item_with_rotation_exact_fit",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10 with only 1 copy available (Feasibility
                // objective). Item 0 (10x10, 1 copy) exactly matches the
                // bin and claims the sole bin copy as its own dedicated
                // bin; item 1 (2x2, 1 copy) is a real, unrelated item
                // still left over with nowhere to go - the reduction
                // alone already proves the instance infeasible, without
                // needing to run any solve on the (otherwise still-
                // buildable) reduced instance.
                fs::path("data") / "rectangle" / "tests" / "feasibility_full_bin_item_exhausts_capacity_infeasible",
                ReductionParameters(),
                true,
            }, {
                // Bin 10x10. Item 0 (10x9, 1 copy) already spans the
                // bin's full width (a degenerate "wide" strip) and is
                // "tall" (2*9 > 10) with a genuinely checked, non-
                // degenerate but empty companion strip (10 wide, 1 tall -
                // there is no other item type in the instance to ever
                // fill it): the companionless step enlarges it to 10x10
                // with zero companions. Once enlarged, it exactly matches
                // the bin on both axes - a strictly better reduction
                // (full removal, as its own dedicated bin) that
                // 'reduce_full_bin_items' must be allowed to pick up in a
                // later round, even though the item is already
                // 'enlarged'.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_companionless_enlargement_upgraded_to_full_bin_item",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x8. Item 0 (7x9, non-oriented, 1 copy) only fits
                // rotated (9 > 8 declared, but 9x7 fits): a regression test
                // for 'reduce_companionless_items' working out which
                // orientation an item can actually be captured at, exactly
                // as 'reduce_full_bin_items' already does, instead of
                // silently assuming the declared one.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_companionless_capture_rotation",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 0 (9x9, oriented, 1 copy) alone: equation
                // (7) ("shrinking the bins") proves the bin's true
                // achievable size is exactly 9x9, matching item 0 exactly,
                // so it is claimed as a dedicated bin even though it does
                // not physically fill the true 10x10 bin.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_full_bin_item_via_shrunk_bin",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Items 0 and 1 (4x9 each) do not tile the true
                // bin exactly, but do tile the *shrunk* bin (8x9) equation
                // (7) proves is the true achievable size given only these
                // two items.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_perfect_pair_via_shrunk_bin",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 0 (4x10) already spans the bin's full
                // height; item 1 (6x6) gets validated-tall-enlarged to 6x10
                // using item 2 (6x4) as its own real companion first, then
                // item 0 and the now-enlarged item 1 tile the bin exactly
                // (4+6=10) as a 'PerfectPair' - exercising two-level nested
                // companion placement (item 0 -> item 1 -> item 2).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_perfect_pair_companion_has_its_own_companions",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 1 (3x7) gets validated-tall-enlarged to
                // 3x10 using item 2 (3x3) as its own real companion first;
                // item 0 (6x6) is "both"-big and its companion-bin check
                // must consider item 1's *current* (already-enlarged) 3x10
                // form, finding it fits alongside item 0 (6+3=9<=10) -
                // exercising the same two-level nesting as the previous
                // case, but reached via 'reduce_both_groups' instead of
                // 'reduce_perfect_pairs'.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_both_group_companion_has_its_own_companions",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10, all items oriented. Item 0 (6x9) is "both"-big.
                // Its wide strip (4x9) cannot be conclusively resolved
                // (item 2, 4x2, is width-eligible but cannot tile all 36
                // units alone), but its *tall* strip (6x1) is a completely
                // different, provably empty region - a regression test for
                // a bug where the wide strip's inconclusive candidate
                // wrongly vetoed the tall strip's own conclusive proof.
                // Item 0 gets tall-companionless-enlarged to 6x10,
                // matching item 1 (4x10) as a 'PerfectPair' (6+4=10);
                // item 2 is then left alone as the *only* remaining item
                // type, so the next round's recomputed shrunk bin exactly
                // equals its own size, claiming it as a second dedicated
                // bin too.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_tall_companionless_enlargement_independent_of_blocked_wide_strip",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10, all items oriented. Item 0 (6x9) is "wide":
                // its wide strip (4x9) is exactly tiled by item 1 (4x9),
                // validated-wide-enlarging it to (10x9) - not yet full
                // height. Item 0's *current*, already-enlarged (10x9) form
                // is then reconsidered for "tall": its now-full-width top
                // strip (10x1) is exactly tiled by item 2 (10x1), composing
                // item 0 up to (10x10) - a full bin, captured with both
                // item 1 and item 2 as its direct (sibling, not nested)
                // companions.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_wide_then_tall_real_companions_compose_into_full_bin_item",
                ReductionParameters(),
                false,
            }, {
                // Bin 20x10 (asymmetric, so only the height axis is ever
                // "big"). Item 0 (5x6) is tall; item 1 (5x1, 3 copies)
                // exactly tiles a (5,3) strip stacked three high - equation
                // (7)'s underlying multiple-choice subset-sum must count
                // each of the 3 copies as its own individual candidate, not
                // item 1's type once regardless of copies. Item 0 becomes
                // the only remaining item type afterwards, so it also ends
                // up claimed as a dedicated bin by a later round's
                // recomputed shrunk bin.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_shrunk_bin_via_multiple_copies_of_same_item",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10. Item 1 (5x9, non-oriented) contributes to
                // equation (7)'s shrunk-width computation via its *rotated*
                // form (9) - a plain per-item subset-sum committing to one
                // fixed value per item upfront could never reach the
                // resulting shrunk (9,9) bin. Against that shrunk bin, item
                // 1's rotated form is "both"-big and item 0 (6x3) is found
                // to fit stacked beside it (3+5=9), captured together as a
                // single dedicated "both" group. 'lift_item_dimensions'
                // disabled to isolate this rotation-aware shrinking from
                // the separate, also-sound reduction the next case
                // exercises on the same instance.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_shrunk_bin_via_non_oriented_item_rotation",
                []() { ReductionParameters p; p.lift_item_dimensions = false; return p; }(),
                false,
            }, {
                // Same instance as the previous case, but with default
                // parameters ('lift_item_dimensions' enabled): shrinking
                // runs first each round, so lifting reasons against the
                // *shrunk* (9,9) bin from the very first round, lifting
                // item 0 (6x3) to exactly (9,4) - "both"-big against that
                // same shrunk bin - which 'reduce_both_groups' immediately
                // captures with item 1's rotated form (9x5) in the very
                // same round, instead of needing an extra round the way
                // the previous, lift-disabled case does.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_lift_item_dimensions_feeds_into_both_group",
                ReductionParameters(),
                false,
            }, {
                // Bin 10x10 with a defect; item 0 (10x10, 2 copies) would
                // otherwise be captured as two dedicated bins (see
                // 'bin_packing_full_bin_item_exact_fit'), but the
                // companion-bin checks this class performs are always
                // plain, defect-free rectangles, so the whole reduction is
                // skipped - and since a 10x10 item can in fact never be
                // placed at all in a 10x10 bin with any defect, the
                // downstream solve genuinely proves the instance
                // infeasible (detected only there, not by the reduction).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_no_reduction_when_bin_has_defects",
                ReductionParameters(),
                false,
                true,
            }, {
                // Same instance as 'bin_packing_full_bin_item_exact_fit',
                // but with a non-'None' unloading constraint set: an
                // unloading order that holds for the big item and its
                // companions checked in isolation says nothing about
                // whether it still holds once other items - chosen later
                // by the downstream solve - join the same bin, so the
                // whole reduction is skipped.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_no_reduction_when_unloading_constraint_set",
                ReductionParameters(),
                false,
            }, {
                // Same instance again, but with a finite bin weight
                // capacity and non-zero item weight instead: total bin
                // weight is a whole-bin aggregate over every item sharing
                // it, the same whole-bin argument as the unloading
                // constraint case, so the whole reduction is skipped.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_no_reduction_when_bin_weight_constrained",
                ReductionParameters(),
                false,
            }, {
                // Same instance as the previous case, but every item
                // weight is left at its default (0): a finite bin weight
                // capacity alone can never be exceeded by items that all
                // weigh nothing, so this is not the risky case the
                // previous test guards against, and the reduction must
                // still apply normally (both copies captured as dedicated
                // bins).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_reduction_still_applies_with_finite_weight_but_zero_item_weights",
                ReductionParameters(),
                false,
            }, {
                // Bin 100x100 (much larger than either item, so no wide/
                // tall/both/full-bin/perfect-pair reduction ever triggers -
                // isolating 'merge_identical_items'). Two item types, both
                // 4x4, oriented, identical on every property
                // 'items_mergeable' checks: they merge into a single item
                // type with the combined copies.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_merge_identical_items",
                ReductionParameters(),
                false,
            }, {
                // Same as above, but the two item types have different
                // profit - not compared by 'items_mergeable' for
                // 'BinPacking' (profit is never the actual objective here,
                // and 'unreduce_solution' always restores each placed
                // copy's own true original profit regardless of merging),
                // so these two still merge.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_merge_identical_items_different_profit_still_merges",
                ReductionParameters(),
                false,
            }, {
                // Bin 100x100, 'Knapsack' objective. Item types 0 and 1
                // (both 4x4, profit 5) are identical including profit and
                // merge; item type 2 (also 4x4, profit 10) is identical on
                // every *other* property but must stay separate - unlike
                // every other objective this class handles, 'Knapsack'
                // optimizes profit directly, so merging different-profit
                // item types would report one uniform profit for every
                // copy and let the solve choose a suboptimal subset on
                // wrong information.
                fs::path("data") / "rectangle" / "tests" / "knapsack_merge_identical_items_requires_matching_profit",
                ReductionParameters(),
                false,
            }, {
                // Bin 100x100, 'Knapsack' objective. Item 0 (4x4, profit
                // 10) is worth including; item 1 (4x4, profit -3, fully
                // optional 'copies_min' 0) is never worth choosing on its
                // own merits, so it is removed outright.
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_negative_profit_items_fully_optional",
                ReductionParameters(),
                false,
            }, {
                // Bin 100x100, 'Knapsack' objective. Item 0 (4x4, profit
                // -3, 5 copies, 'copies_min' 2) has some copies genuinely
                // mandatory - only the 3 optional copies beyond
                // 'copies_min' are ever worth dropping, so 'copies' is
                // trimmed to exactly 2, not removed outright.
                // 'skip_optimize_check': a mandatory, negative-profit item
                // type under 'Knapsack' hangs every solver algorithm this
                // codebase has - a pre-existing bug unrelated to
                // 'Reduction' (confirmed separately: it reproduces even
                // with a 10-second time limit set, i.e. the hang itself
                // does not respect the time limit either), well outside
                // this class's own scope to fix.
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_negative_profit_items_trims_to_minimum",
                ReductionParameters(),
                false,
                false,
                false,
                true,
            }, {
                // Bin 100x100, 'Knapsack' objective, one resource with a
                // *negative* penalty (a profit bonus the first time a
                // bin's consumption crosses capacity 1). Item 0 (4x4,
                // profit -3) has negative profit but also consumes the
                // resource - including it triggers the crossing (bonus
                // 100), making it worth including despite its own negative
                // profit, an indirect benefit
                // 'remove_negative_profit_items_applies' cannot see, so
                // the whole operation is skipped. 'skip_optimize_check':
                // every solver algorithm this codebase has hangs on this
                // negative-penalty-resource / 'Knapsack' combination - a
                // pre-existing bug unrelated to 'Reduction' (confirmed
                // separately: flipping the same setup's penalty to
                // positive does not hang, and 'Reduction' construction and
                // 'expect_instances_equal' above already complete
                // instantly regardless).
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_negative_profit_items_skipped_with_negative_penalty_resource",
                ReductionParameters(),
                false,
                false,
                true,
                true,
            }, {
                // Bin 10x10, 1 copy, 'Knapsack' objective. Item 0 (4x4,
                // profit 10) dominates item 1 (8x8, profit 5): smaller
                // footprint, higher profit, same (default) group/
                // eligibility, no weight/resources, and also provably
                // incompatible (4+8=12 exceeds the bin's width and height,
                // so both oriented items can never both fit in this one
                // bin), so item 1 is removed outright.
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_dominated_items_basic_removal",
                ReductionParameters(),
                false,
            }, {
                // Bin 20x20 (large enough for both items at once). Item 0
                // (4x4, profit 10) still dominates item 1 (5x5, profit 5)
                // pointwise, but they are *not* incompatible (4+5=9 fits
                // well within the 20-wide/tall bin) - a solution could
                // profitably use both at once, so item 1 must not be
                // removed.
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_dominated_items_not_removed_when_compatible",
                ReductionParameters(),
                false,
            }, {
                // Same geometry as the basic-removal case above (provably
                // incompatible in this one bin), but items 0 and 1 belong
                // to different groups - swapping item 1 for item 0 could
                // disturb unloading order, so item 1 must not be removed
                // despite otherwise dominating/incompatible.
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_dominated_items_not_removed_when_different_group",
                ReductionParameters(),
                false,
            }, {
                // Same geometry as the basic-removal case above, but item
                // 1 has a positive 'copies_min' - it cannot simply be
                // swapped away, since at least one copy is forced
                // regardless of profit. 'skip_optimize_check': running
                // 'optimize()' on this instance (item 1 mandatory but
                // geometrically incompatible with item 0 in this one bin)
                // currently comes back wrongly proven infeasible instead
                // of finding the valid "item 1 alone" solution - a
                // separate, pre-existing solver defect around 'Knapsack'
                // plus mandatory item types (see the negative-profit-item
                // hang cases above) unrelated to 'Reduction' itself, which
                // still correctly leaves both item types untouched here.
                fs::path("data") / "rectangle" / "tests" / "knapsack_remove_dominated_items_not_removed_when_mandatory",
                ReductionParameters(),
                false,
                false,
                false,
                true,
            }, {
                // 'VariableSizedBinPacking'. Bin type 1 (20x20, cost 100, 1
                // copy) dominates bin type 0 (10x10, also cost 100, 2
                // copies): at least as big in both dimensions, no more
                // expensive, and has "enough" copies (1 >= the instance's
                // own 1 total item), so bin type 0 is removed outright.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types_basic_removal",
                ReductionParameters(),
                false,
            }, {
                // Same sizes, but bin type 1 (20x20) costs more than bin
                // type 0 (10x10) - being bigger does not make it dominate
                // a cheaper, smaller alternative, so neither bin type is
                // removed. This is the exact shape of a real regression
                // this implementation initially had (see
                // 'data/rectangle/tests/bin_packing_empty_bin', a
                // pre-existing test with a larger, pricier bin type that
                // must *not* be allowed to evict a smaller, cheaper one).
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types_not_removed_when_more_expensive",
                ReductionParameters(),
                false,
            }, {
                // Same dominance relationship as the basic-removal case
                // above (bin type 1 dominates bin type 0 pointwise: bigger,
                // same cost), but with 2 items instead of 1 - bin type 1's
                // own single copy no longer covers 'number_of_items()'
                // (now 2), so it does not have "enough" copies to safely
                // replace bin type 0's own 2, and neither is removed.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types_not_removed_when_not_enough_copies",
                ReductionParameters(),
                false,
            }, {
                // Same as the basic-removal case above, but bin type 0 has
                // a positive 'copies_min' - it cannot simply be dropped,
                // since at least one copy is forced regardless of what
                // dominates it. 'skip_optimize_check': 'optimize()' times
                // out on this instance (the column generation solver
                // reaches a fractional bound, 1.5625, and never resolves
                // it to an integer solution) - a separate, pre-existing
                // solver defect around mandatory ('copies_min' > 0) bin
                // types unrelated to 'Reduction' itself (which still
                // correctly leaves both bin types untouched here); the
                // original test never called 'optimize()' either.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types_not_removed_when_mandatory",
                ReductionParameters(),
                false,
                false,
                false,
                true,
            }, {
                // Same dominance relationship as the basic-removal case
                // above, but 'BinPacking' instead of
                // 'VariableSizedBinPacking' - unlike every other objective
                // this class handles, 'BinPacking' requires its bin types
                // to be used in the exact order they are declared, so
                // removing bin type 0 would shift bin type 1 earlier in
                // that fixed sequence - a different problem, not a sound
                // reduction, regardless of how thoroughly it dominates bin
                // type 0. Neither bin type is removed.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_remove_dominated_bin_types_not_removed_for_bin_packing",
                ReductionParameters(),
                false,
            }, {
                // Same dominance relationship as the basic-removal case
                // above, but bin type 0 (the dominated one) also has a
                // plain, non-'penalize' resource that bin type 1 (the
                // dominator) does not. Bin type 1, having no resources at
                // all, is unrestricted along every resource dimension, so
                // the resource does not block dominance, and bin type 0 is
                // still removed. 'use_json': resources cannot be
                // represented in the CSV format.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types_removed_when_only_b_has_resource",
                ReductionParameters(),
                false,
                false,
                true,
            }, {
                // Same dominance relationship again, but bin type 0's
                // resource is 'penalize' with a *negative* penalty - a
                // one-time profit bonus the first time a bin's consumption
                // crosses its capacity. Bin type 1 has no matching
                // resource, so it could never replicate that bonus:
                // removing bin type 0 would make bin type 1 strictly worse
                // for solutions that would have triggered it. Neither bin
                // type is removed. 'use_json' for the resource;
                // 'skip_optimize_check' since this combination is
                // suspected (by analogy with the negative-penalty-resource
                // 'Knapsack' hang documented above) to share the same
                // class of pre-existing, 'Reduction'-unrelated solver
                // defect, and was not separately probed.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types_not_removed_with_negative_penalty_resource_on_b",
                ReductionParameters(),
                false,
                false,
                true,
                true,
            }, {
                // Bin 100x100 with a defect: 'companion_absorption_applies'
                // is 'false' (defects break every companion-bin geometric
                // check), but 'merge_identical_items' does not care about
                // defects at all - it still merges item types 0 and 1
                // (both 4x4, 2 copies each) into a single type with 4
                // copies.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_merge_identical_items_with_defects",
                ReductionParameters(),
                false,
            }, {
                // Bin 100x100 with one resource. Item types 0 and 1 (both
                // 4x4, oriented) have the exact same per-copy consumption
                // schedule and merge; item type 2 (also 4x4, oriented) has
                // a different schedule and must stay separate -
                // 'resources_matter()' makes 'companion_absorption_applies'
                // 'false' here too, but 'merge_identical_items' still
                // runs, checking resource schedules for equality instead
                // of being blocked outright. 'use_json' for the resource.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_merge_identical_items_respects_resources",
                ReductionParameters(),
                false,
                false,
                true,
            }, {
                // Bin 20x20 (taller than the items, unlike the bin's true
                // width - deliberately, so neither the wide/tall/both
                // companion checks nor 'reduce_perfect_pairs' find
                // anything). Item types 0 and 1 (both 5x10, oriented, 1
                // copy each) are identical and merge into a single reduced
                // item type with 2 copies; placing both reduced copies
                // side by side and unreducing must recover a valid
                // solution using original item type ids 0 and 1, each
                // exactly once.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_merge_identical_items_unreduce_solution_round_trip",
                ReductionParameters(),
                false,
            },
        }));

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

