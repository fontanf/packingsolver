#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"
#include "packingsolver/boxstacks/reduction.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace packingsolver;
using namespace packingsolver::boxstacks;

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
        EXPECT_EQ(actual_bin_type.box.x, expected_bin_type.box.x);
        EXPECT_EQ(actual_bin_type.box.y, expected_bin_type.box.y);
        EXPECT_EQ(actual_bin_type.box.z, expected_bin_type.box.z);
        EXPECT_EQ(actual_bin_type.copies, expected_bin_type.copies);
        EXPECT_EQ(actual_bin_type.copies_min, expected_bin_type.copies_min);
    }

    ASSERT_EQ(actual.number_of_item_types(), expected.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < expected.number_of_item_types();
            ++item_type_id) {
        const ItemType& actual_item_type = actual.item_type(item_type_id);
        const ItemType& expected_item_type = expected.item_type(item_type_id);
        EXPECT_EQ(actual_item_type.box.x, expected_item_type.box.x);
        EXPECT_EQ(actual_item_type.box.y, expected_item_type.box.y);
        EXPECT_EQ(actual_item_type.box.z, expected_item_type.box.z);
        EXPECT_EQ(actual_item_type.copies, expected_item_type.copies);
        EXPECT_EQ(actual_item_type.copies_min, expected_item_type.copies_min);
        EXPECT_EQ(actual_item_type.profit, expected_item_type.profit);
        EXPECT_EQ(actual_item_type.rotations, expected_item_type.rotations);
        EXPECT_EQ(actual_item_type.weight, expected_item_type.weight);
        EXPECT_EQ(actual_item_type.group_id, expected_item_type.group_id);
        EXPECT_EQ(actual_item_type.stackability_id, expected_item_type.stackability_id);
        EXPECT_EQ(actual_item_type.nesting_height, expected_item_type.nesting_height);
        EXPECT_EQ(actual_item_type.maximum_stackability, expected_item_type.maximum_stackability);
        EXPECT_EQ(actual_item_type.maximum_weight_above, expected_item_type.maximum_weight_above);
    }
}

}

/**
 * One 'Reduction' scenario, read from 'data/boxstacks/tests/<name>/' -
 * mirrors 'RectangleReductionTestParams' (see 'test/rectangle/reduction_test.cpp'),
 * minus 'expected_proven_infeasible'/'expected_output_infeasible': unlike
 * 'rectangle::Reduction', 'boxstacks::Reduction' can never prove infeasibility
 * or hide items in a dedicated bin on its own (see its own class-level doc
 * comment), so 'instance()' is always meaningful and there is nothing of that
 * kind to assert on here.
 */
struct BoxStacksReductionTestParams
{
    fs::path dir;
    ReductionParameters reduction_parameters;
    /**
     * Do not call 'optimize()' at all (so no 'solution.csv' is read or
     * needed) - mirrors 'RectangleReductionTestParams''s own field, for
     * scenarios that exercise 'Reduction' itself against a combination this
     * codebase's solvers cannot currently handle downstream.
     */
    bool skip_optimize_check = false;
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
        const BoxStacksReductionTestParams& test_params)
{
    os << test_params.dir;
    return os;
}

class BoxStacksReductionTest: public testing::TestWithParam<BoxStacksReductionTestParams> { };

TEST_P(BoxStacksReductionTest, BoxStacksReduction)
{
    BoxStacksReductionTestParams test_params = GetParam();

    InstanceBuilder instance_builder;
    instance_builder.read_item_types((test_params.dir / "items.csv").string());
    instance_builder.read_bin_types((test_params.dir / "bins.csv").string());
    instance_builder.read_parameters((test_params.dir / "parameters.csv").string());
    Instance instance = instance_builder.build();

    Reduction reduction(instance, test_params.reduction_parameters);

    InstanceBuilder expected_instance_builder;
    expected_instance_builder.read_item_types((test_params.dir / "reduced_items.csv").string());
    expected_instance_builder.read_bin_types((test_params.dir / "reduced_bins.csv").string());
    expected_instance_builder.read_parameters((test_params.dir / "parameters.csv").string());
    Instance expected_instance = expected_instance_builder.build();
    expect_instances_equal(reduction.instance(), expected_instance);

    if (test_params.skip_optimize_check)
        return;

    OptimizeParameters optimize_parameters;
    optimize_parameters.reduction_parameters = test_params.reduction_parameters;
    boxstacks::Output output = optimize(instance, optimize_parameters);

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

INSTANTIATE_TEST_SUITE_P(
        BoxStacks,
        BoxStacksReductionTest,
        testing::ValuesIn(std::vector<BoxStacksReductionTestParams>{
            {
                // Bin 1000x1000x1000 (much larger than either item, so
                // every item fits in its own stack side by side - no
                // stacking-feasibility complications). Two item types, both
                // 100x100x100, identical on every property 'items_mergeable'
                // checks: they merge into a single item type with the
                // combined copies (3 + 2 = 5).
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items",
                ReductionParameters(),
            }, {
                // Same as above, but the two item types have different
                // profit - not compared by 'items_mergeable' for
                // 'BinPacking' (profit is never the actual objective here,
                // and 'unreduce_solution' always restores each placed
                // copy's own true original profit regardless of merging),
                // so these two still merge.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items_different_profit_still_merges",
                ReductionParameters(),
            }, {
                // 'Knapsack' objective. Item types 0 and 1 (both
                // 100x100x100, profit 5) are identical including profit and
                // merge; item type 2 (also 100x100x100, profit 10) is
                // identical on every *other* property but must stay
                // separate - 'Knapsack' optimizes profit directly, so
                // merging different-profit item types would report one
                // uniform profit for every copy and let the solve choose a
                // suboptimal subset on wrong information.
                fs::path("data") / "boxstacks" / "tests" / "knapsack_merge_identical_items_requires_matching_profit",
                ReductionParameters(),
            }, {
                // 'Knapsack' objective. Item 0 (100x100x100, profit 10) is
                // worth including; item 1 (100x100x100, profit -3, fully
                // optional 'copies_min' 0) is never worth choosing on its
                // own merits, so it is removed outright.
                fs::path("data") / "boxstacks" / "tests" / "knapsack_remove_negative_profit_items_fully_optional",
                ReductionParameters(),
            }, {
                // 'Knapsack' objective. Item 0 (100x100x100, profit -3, 5
                // copies, 'copies_min' 2) has some copies genuinely
                // mandatory - only the 3 optional copies beyond
                // 'copies_min' are ever worth dropping, so 'copies' is
                // trimmed to exactly 2, not removed outright.
                // 'skip_optimize_check': mirrors rectangle's own
                // 'knapsack_remove_negative_profit_items_trims_to_minimum'
                // ('optimize()' on a mandatory, negative-profit 'Knapsack'
                // item type times out here too - a pre-existing,
                // 'Reduction'-unrelated solver limitation, confirmed
                // separately with a 5-second time limit) - only the
                // reduced-instance check runs.
                fs::path("data") / "boxstacks" / "tests" / "knapsack_remove_negative_profit_items_trims_to_minimum",
                ReductionParameters(),
                true,
            }, {
                // Bin large enough for both items side by side. Item types
                // 0 and 1 (both 100x100x100, oriented, same weight/
                // stackability) have different 'group_id' - must not
                // merge, unlike rectangle's resource-schedule equivalent
                // ('boxstacks' has no resources at all - see
                // 'Reduction''s own class-level doc comment).
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items_requires_matching_group",
                ReductionParameters(),
            }, {
                // Same shape, but item types 0 and 1 differ on
                // 'stackability_id' instead of 'group_id' - must not merge
                // either, even though every other field (including
                // dimensions) matches.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items_requires_matching_stackability",
                ReductionParameters(),
            }, {
                // Item types 0 and 1 have identical dimensions but
                // different allowed-rotation sets (item 0: XYZ only; item
                // 1: XYZ and YXZ) - must not merge, since a downstream
                // solve for the reduced instance's own single merged type
                // would only ever see one of the two rotation policies.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items_requires_matching_rotations",
                ReductionParameters(),
            }, {
                // Bin large enough for both items side by side but not
                // stacked (no wide/tall/companion mechanism exists for this
                // problem type at all, so nothing else could ever interfere
                // with isolating 'merge_identical_items' here). Item types
                // 0 and 1 (both 200x150x100, oriented, 1 copy each) are
                // identical and merge into a single reduced item type with
                // 2 copies; placing both reduced copies side by side and
                // unreducing must recover a valid solution using original
                // item type ids 0 and 1, each exactly once.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items_unreduce_solution_round_trip",
                ReductionParameters(),
            }, {
                // Bin 100x100x100, holding exactly one item per bin. Item
                // types 0 (30 copies) and 1 (20 copies), both
                // 100x100x100, merge into a single reduced item type with
                // 50 copies. Regression test: 'unreduce_solution' must
                // recognize that every physical bin resolving to original
                // item type 0 forms one contiguous run (and likewise for
                // type 1), and group them into exactly 2 'add_bin'
                // entries (30 copies then 20, or vice versa) - not one
                // entry per physical bin (50), which is what an earlier,
                // unconditionally-splitting version of this class
                // actually did whenever a merged pattern was reused
                // across more than one physical bin.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_merge_identical_items_unequal_copies_produces_compact_groups",
                ReductionParameters(),
                false,
                2,
            },
        }));
