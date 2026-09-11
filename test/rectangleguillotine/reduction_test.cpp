#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "packingsolver/rectangleguillotine/reduction.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

/**
 * Structural comparison of every property 'Reduction' can affect, mirroring
 * 'rectangle::reduction_test.cpp''s own 'expect_instances_equal'.
 */
void expect_instances_equal(const Instance& actual, const Instance& expected)
{
    ASSERT_EQ(actual.number_of_bin_types(), expected.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < expected.number_of_bin_types();
            ++bin_type_id) {
        const BinType& actual_bin_type = actual.bin_type(bin_type_id);
        const BinType& expected_bin_type = expected.bin_type(bin_type_id);
        EXPECT_EQ(actual_bin_type.rect.w, expected_bin_type.rect.w);
        EXPECT_EQ(actual_bin_type.rect.h, expected_bin_type.rect.h);
        EXPECT_EQ(actual_bin_type.copies, expected_bin_type.copies);
        EXPECT_EQ(actual_bin_type.copies_min, expected_bin_type.copies_min);
    }

    ASSERT_EQ(actual.number_of_item_types(), expected.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < expected.number_of_item_types();
            ++item_type_id) {
        const ItemType& actual_item_type = actual.item_type(item_type_id);
        const ItemType& expected_item_type = expected.item_type(item_type_id);
        EXPECT_EQ(actual_item_type.rect.w, expected_item_type.rect.w);
        EXPECT_EQ(actual_item_type.rect.h, expected_item_type.rect.h);
        EXPECT_EQ(actual_item_type.oriented, expected_item_type.oriented);
        EXPECT_EQ(actual_item_type.copies, expected_item_type.copies);
        EXPECT_EQ(actual_item_type.copies_min, expected_item_type.copies_min);
        EXPECT_EQ(actual_item_type.profit, expected_item_type.profit);
    }
}

}

/**
 * One 'Reduction' scenario, read from 'data/rectangleguillotine/tests/<name>/'
 * - mirrors 'rectangle::RectangleReductionTestParams''s own doc comment.
 * Unlike 'rectangle::Reduction', this class never proves infeasibility on
 * its own, so there is no 'expected_proven_infeasible' field here.
 */
struct RectangleGuillotineReductionTestParams
{
    fs::path dir;
    ReductionParameters reduction_parameters;
    /**
     * Do not call 'optimize()' at all (so no 'solution.csv' is read or
     * needed): matches 'RectangleReductionTestParams::skip_optimize_check'
     * - used for scenarios exercising a pre-existing, 'Reduction'-unrelated
     * solver defect (e.g. a mandatory negative-profit item type under
     * 'Knapsack').
     */
    bool skip_optimize_check = false;
    /** Needs a resource, not representable in the plain CSV format. */
    bool use_json = false;
    /**
     * When non-negative, the exact 'Solution::number_of_different_bins()'
     * the unreduced solution must have - catches 'unreduce_solution'
     * exploding a pattern shared by many physical bins into one entry per
     * physical copy instead of grouping consecutive copies that resolved
     * to the same original item ids.
     */
    BinPos expected_number_of_different_bins = -1;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleGuillotineReductionTestParams& test_params)
{
    os << test_params.dir;
    return os;
}

class RectangleGuillotineReductionTest: public testing::TestWithParam<RectangleGuillotineReductionTestParams> { };

TEST_P(RectangleGuillotineReductionTest, RectangleGuillotineReduction)
{
    RectangleGuillotineReductionTestParams test_params = GetParam();

    InstanceBuilder instance_builder;
    if (test_params.use_json) {
        instance_builder.read((test_params.dir / "instance.json").string());
    } else {
        instance_builder.read_item_types((test_params.dir / "items.csv").string());
        instance_builder.read_bin_types((test_params.dir / "bins.csv").string());
        instance_builder.read_parameters((test_params.dir / "parameters.csv").string());
    }
    Instance instance = instance_builder.build();

    Reduction reduction(instance, test_params.reduction_parameters);

    InstanceBuilder expected_instance_builder;
    if (test_params.use_json) {
        expected_instance_builder.read((test_params.dir / "reduced_instance.json").string());
    } else {
        expected_instance_builder.read_item_types((test_params.dir / "reduced_items.csv").string());
        expected_instance_builder.read_bin_types((test_params.dir / "reduced_bins.csv").string());
        expected_instance_builder.read_parameters((test_params.dir / "parameters.csv").string());
    }
    Instance expected_instance = expected_instance_builder.build();
    expect_instances_equal(reduction.instance(), expected_instance);

    if (test_params.skip_optimize_check)
        return;

    OptimizeParameters optimize_parameters;
    optimize_parameters.reduction_parameters = test_params.reduction_parameters;
    rectangleguillotine::Output output = optimize(instance, optimize_parameters);

    Solution solution = output.solution_pool.best();

    SolutionBuilder expected_solution_builder(instance);
    expected_solution_builder.read((test_params.dir / "solution.csv").string());
    Solution expected_solution = expected_solution_builder.build();

    EXPECT_TRUE(!(solution < expected_solution));
    EXPECT_TRUE(!(expected_solution < solution));

    if (test_params.expected_number_of_different_bins >= 0) {
        EXPECT_EQ(
                solution.number_of_different_bins(),
                test_params.expected_number_of_different_bins);
    }
}

TEST(RectangleGuillotineReduction, MergeUnequalCopiesProducesCompactGroups)
{
    // Bin 4x4, 50 copies, holding exactly one item per bin. Item types 0
    // (30 copies) and 1 (20 copies), both 4x4, oriented, each alone in its
    // own auto-assigned singleton stack, merge into a single reduced item
    // type with 50 copies. A reduced solution placing one reduced copy per
    // bin, all 50 bins sharing one 'SolutionBin' entry with 'copies = 50'
    // (exactly like a real solve would report a pattern reused across many
    // physical bins) - built directly via 'SolutionBuilder', not through a
    // full 'optimize()' solve, to test 'unreduce_solution' in isolation
    // (the cut-tree CSV certificate format used by the parameterized cases
    // above is impractical to hand-write at this scale). Regression test:
    // 'unreduce_solution' must recognize that the physical bins resolving
    // to original item type 0 form one contiguous run (and likewise for
    // type 1), and group them into exactly 2 'add_bin' entries (30 copies
    // then 20, or vice versa) - not one entry per physical bin (50), which
    // is what an earlier, unconditionally-splitting version of this class
    // actually did whenever a merged pattern was reused across more than
    // one physical bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(4, 4);
    instance_builder.set_bin_type_copies(bin_type_id, 50);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(4, 4, true);
    instance_builder.set_item_type_copies(item_type_id_0, 30);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(4, 4, true);
    instance_builder.set_item_type_copies(item_type_id_1, 20);
    instance_builder.set_first_stage_orientation(CutOrientation::Vertical);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    ASSERT_EQ(reduced_instance.item_type(0).copies, 50);

    SolutionBuilder solution_builder(reduced_instance);
    solution_builder.add_bin(0, 50, CutOrientation::Vertical);
    solution_builder.add_node(1, 4);
    solution_builder.set_last_node_item(0);
    Solution reduced_solution = solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_EQ(solution.number_of_different_bins(), 2);
    ItemPos copies_0 = 0;
    ItemPos copies_1 = 0;
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        ItemTypeId placed_item_type_id = -1;
        for (const SolutionNode& node: solution_bin.nodes) {
            if (node.item_type_id >= 0)
                placed_item_type_id = node.item_type_id;
        }
        if (placed_item_type_id == item_type_id_0)
            copies_0 += solution_bin.copies;
        else if (placed_item_type_id == item_type_id_1)
            copies_1 += solution_bin.copies;
    }
    EXPECT_EQ(copies_0, 30);
    EXPECT_EQ(copies_1, 20);
}

INSTANTIATE_TEST_SUITE_P(
        RectangleGuillotine,
        RectangleGuillotineReductionTest,
        testing::ValuesIn(std::vector<RectangleGuillotineReductionTestParams>{
            {
                // Bin 8x4, 10 copies. Two item types, both 4x4, oriented,
                // identical on every property 'items_mergeable' checks
                // (each alone in its own auto-assigned singleton stack -
                // no 'STACK_ID' column): they merge into a single item
                // type with the combined copies (3 + 2 = 5).
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_merge_identical_items",
                ReductionParameters(),
            }, {
                // Same geometry, but item types 1/2 (both 5x10) have
                // different profit (5 vs 10) - not compared by
                // 'items_mergeable' for 'BinPacking' (profit is never the
                // actual objective here, and 'unreduce_solution' always
                // restores each placed copy's own true original profit
                // regardless of merging), so they still merge, taking the
                // survivor's (item 1's) own profit.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_merge_identical_items_different_profit_still_merges",
                ReductionParameters(),
            }, {
                // Bin 20x20, 'Knapsack' objective, every item type fully
                // optional ('copies_min' 0). Item types 0 and 1 (both 5x10,
                // profit 5) are identical including profit and merge; item
                // type 2 (also 5x10, profit 10) is identical on every
                // *other* property but must stay separate - unlike every
                // other objective this class handles, 'Knapsack' optimizes
                // profit directly, so merging different-profit item types
                // would report one uniform profit for every copy and let
                // the solve choose a suboptimal subset on wrong
                // information. Item type 3 (10x10) is unrelated filler.
                // Every item type's profit is positive and the bin has
                // plenty of room, so the optimal solution places all of
                // them.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_merge_identical_items_requires_matching_profit",
                ReductionParameters(),
            }, {
                // Bin 20x20, 'Knapsack' objective. Item 0 (5x10, profit 10)
                // is worth including; item 1 (5x10, profit -3, fully
                // optional 'copies_min' 0) is never worth choosing on its
                // own merits, so it is removed outright.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_remove_negative_profit_items_fully_optional",
                ReductionParameters(),
            }, {
                // Bin 20x20, 'Knapsack' objective. Item 0 (5x10, profit -3,
                // 5 copies, 'copies_min' 2) has some copies genuinely
                // mandatory - only the 3 optional copies beyond
                // 'copies_min' are ever worth dropping, so 'copies' is
                // trimmed to exactly 2, not removed outright.
                // 'skip_optimize_check': a mandatory, negative-profit item
                // type under 'Knapsack' is wrongly reported proven
                // infeasible by every solver algorithm this codebase has
                // when run directly on this instance (confirmed
                // separately, with '--reduce false' semantics implied by
                // running 'optimize()' on the plain, unreduced instance) -
                // a pre-existing bug unrelated to 'Reduction' (matches the
                // same class of issue 'rectangle::reduction_test.cpp'
                // documents for its own identical scenario, there
                // manifesting as a hang instead of a wrong answer).
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_remove_negative_profit_items_trims_to_minimum",
                ReductionParameters(),
                true,
            }, {
                // Bin 100x100 with one resource. Item types 0 and 1 (both
                // 4x4, oriented) have the exact same per-copy consumption
                // schedule and merge; item type 2 (also 4x4, oriented) has
                // a different schedule and must stay separate.
                // 'use_json' for the resource; 'skip_optimize_check' since
                // this scenario is only exercising the merge decision
                // itself (verified via 'expect_instances_equal'), not a
                // full geometric solve.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_merge_identical_items_respects_resources",
                ReductionParameters(),
                true,
                true,
            }, {
                // Bin 20x10 (exactly twice as wide as each item, same
                // height - no wide/tall/companion machinery here at all,
                // this class only ever does two things). Item types 0 and
                // 1 (both 10x10, oriented, 1 copy each) are identical and
                // merge into a single reduced item type with 2 copies;
                // placing both reduced copies side by side and unreducing
                // must recover a valid solution using original item type
                // ids 0 and 1, each exactly once. This is an especially
                // important scenario for this problem type specifically:
                // 'unreduce_solution' here replays a cut-tree node-by-node
                // (not a flat item list like 'rectangle'), so this
                // exercises that replay logic directly, with a bin that
                // places more than one item.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_merge_identical_items_unreduce_solution_round_trip",
                ReductionParameters(),
            }, {
                // 'rectangleguillotine'-specific: no 'rectangle' equivalent.
                // Bin 20x10. Two item types, both 10x10, oriented,
                // identical on every other property 'items_mergeable'
                // checks - but both explicitly assigned to the *same*
                // 'STACK_ID' (0), so 'stack_size(0)' is 2, not either
                // item type's own 1 copy: neither is "alone" in its stack,
                // so merging them could silently disturb that stack's
                // cutting-order sequencing and must not happen. The
                // reduced instance comes back identical to the input,
                // untouched. 'skip_optimize_check': only the merge
                // decision itself is being exercised here.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_merge_identical_items_not_applied_when_sharing_stack",
                ReductionParameters(),
                true,
            }, {
                // 'rectangleguillotine'-specific: no 'rectangle' equivalent.
                // Bin 20x20, 'Knapsack' objective, three item types (all
                // 5x10, profit/copies varying). Item type 0 (profit -3, 5
                // copies, 'copies_min' 2) is alone in its own stack (id 0)
                // - 'stack_size(0)' equals its own 5 copies exactly - so it
                // is safely trimmed down to 'copies_min' (2), exactly like
                // 'knapsack_remove_negative_profit_items_trims_to_minimum'
                // above. Item type 1 (profit -3, 5 copies, 'copies_min' 2)
                // looks identical, but shares stack id 1 with item type 2
                // (profit 1, 1 copy): 'stack_size(1)' is 6, not item type
                // 1's own 5 copies, so it is *not* alone, and must be left
                // untouched at its original 5 copies - locking in a real
                // bug fixed earlier in this same session where
                // 'remove_negative_profit_items' lacked this guard
                // entirely (it would have wrongly trimmed item type 1 down
                // to 2 here too, silently weakening the "fully place item
                // type 1 before item type 2's own stack position" ordering
                // requirement). 'skip_optimize_check': same pre-existing,
                // 'Reduction'-unrelated mandatory-negative-profit/'Knapsack'
                // solver defect as the scenario above.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_remove_negative_profit_items_skipped_when_sharing_stack",
                ReductionParameters(),
                true,
            }, {
                // 'rectangleguillotine'-specific: regression test for a bug
                // caught while writing this suite (fixed alongside these
                // tests). Bin 20x10. Two item types, both 10x10, oriented,
                // each alone in its own auto-assigned singleton stack, but
                // with more than one copy each (3 and 2) - unlike every
                // other scenario in this suite, where every merge candidate
                // happens to have exactly 1 copy. 'stack_size' counts every
                // *physical copy* in a stack, not distinct item types, so
                // checking it against a literal '1' (as this class
                // originally did) would wrongly conclude a lone item type
                // with more than one copy of its own is "not alone" and
                // block the merge; the correct check compares against the
                // item type's own copies instead. They still merge into a
                // single reduced item type with the combined 5 copies.
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_merge_identical_items_alone_in_stack_with_multiple_copies",
                ReductionParameters(),
                true,
            },
        }));
