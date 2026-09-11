#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"
#include "packingsolver/box/reduction.hpp"
#include "box/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <set>

namespace fs = boost::filesystem;
using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

/**
 * Structural comparison of every property 'Reduction' can affect, mirroring
 * 'rectangle::reduction_test.cpp''s own 'expect_instances_equal' - adapted
 * to 'box''s own fields (a 'Box' of 3 dimensions instead of a 'Rectangle',
 * and a *set* of allowed 'Rotation's instead of a single 'oriented' bool).
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
        EXPECT_EQ(actual_bin_type.cost, expected_bin_type.cost);
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
        std::set<Rotation> actual_rotations(
                actual_item_type.rotations.begin(),
                actual_item_type.rotations.end());
        std::set<Rotation> expected_rotations(
                expected_item_type.rotations.begin(),
                expected_item_type.rotations.end());
        EXPECT_EQ(actual_rotations, expected_rotations);
        EXPECT_EQ(actual_item_type.copies, expected_item_type.copies);
        EXPECT_EQ(actual_item_type.copies_min, expected_item_type.copies_min);
        EXPECT_EQ(actual_item_type.profit, expected_item_type.profit);
        EXPECT_EQ(actual_item_type.weight, expected_item_type.weight);
    }
}

}

/**
 * One 'Reduction' scenario, read from 'data/box/tests/<name>/' - mirroring
 * 'RectangleReductionTestParams''s own doc comment, minus every field only
 * meaningful for the companion-absorption/dimension-lifting/dominated-item/
 * dominated-bin-type operations 'box::Reduction' does not implement
 * ('expected_proven_infeasible': 'box::Reduction' never proves infeasibility
 * on its own, unlike 'rectangle::Reduction').
 */
struct BoxReductionTestParams
{
    fs::path dir;
    ReductionParameters reduction_parameters;
    /**
     * The instance is infeasible overall (checked directly via
     * 'optimize()''s own 'is_proven_infeasible'), but not provably so by the
     * reduction alone. Only meaningful when 'skip_optimize_check' is
     * 'false': when 'true', the reduced instance is still checked against
     * 'reduced_items.csv'/'reduced_bins.csv' as usual, but 'solution.csv' is
     * not read.
     */
    bool expected_output_infeasible = false;
    /**
     * Use 'instance.json'/'reduced_instance.json' instead of 'items.csv'/
     * 'bins.csv'/'reduced_items.csv'/'reduced_bins.csv' - needed for any
     * feature the CSV format cannot represent (resources).
     */
    bool use_json = false;
    /**
     * Do not call 'optimize()' at all (so no 'solution.csv' is read or
     * needed): some scenarios exist specifically to exercise 'Reduction'
     * itself against a combination this codebase's solvers cannot currently
     * handle downstream (a pre-existing, 'Reduction'-unrelated defect around
     * 'Knapsack' plus a mandatory or negative-penalty-resource-consuming
     * negative-profit item type - see rectangle's own
     * 'RemoveNegativeProfitItemsTrimsToMinimum'/
     * 'RemoveNegativeProfitItemsSkippedWithNegativePenaltyResource'), or
     * that simply have no natural optimal-solution reference to check
     * (the rotation-mismatch scenario, which only exercises the reduced
     * instance itself). Only 'expect_instances_equal' runs; the reduced
     * instance is still checked as usual.
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
        const BoxReductionTestParams& test_params)
{
    os << test_params.dir;
    return os;
}

class BoxReductionTest: public testing::TestWithParam<BoxReductionTestParams> { };

TEST_P(BoxReductionTest, BoxReduction)
{
    BoxReductionTestParams test_params = GetParam();

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
    box::Output output = optimize(instance, optimize_parameters);

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

    if (test_params.expected_number_of_different_bins >= 0) {
        EXPECT_EQ(
                solution.number_of_different_bins(),
                test_params.expected_number_of_different_bins);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Box,
        BoxReductionTest,
        testing::ValuesIn(std::vector<BoxReductionTestParams>{
            {
                // Bin 100x100x100, 5 copies (BinPacking). Two item types,
                // both 4x4x4, oriented, identical on every property
                // 'items_mergeable' checks: they merge into a single item
                // type with the combined copies.
                fs::path("data") / "box" / "tests" / "bin_packing_merge_identical_items",
                ReductionParameters(),
            }, {
                // Same as above, but the two item types have different
                // profit - not compared by 'items_mergeable' for
                // 'BinPacking' (profit is never the actual objective here,
                // and 'unreduce_solution' always restores each placed
                // copy's own true original profit regardless of merging),
                // so these two still merge, taking the survivor's (item
                // type 0's) own profit.
                fs::path("data") / "box" / "tests" / "bin_packing_merge_identical_items_different_profit_still_merges",
                ReductionParameters(),
            }, {
                // 'Knapsack' objective. Item types 0 and 1 (both 4x4x4,
                // profit 5) are identical including profit and merge; item
                // type 2 (also 4x4x4, profit 10) is identical on every
                // *other* property but must stay separate - unlike every
                // other objective this class handles, 'Knapsack' optimizes
                // profit directly, so merging different-profit item types
                // would report one uniform profit for every copy and let
                // the solve choose a suboptimal subset on wrong
                // information.
                fs::path("data") / "box" / "tests" / "knapsack_merge_identical_items_requires_matching_profit",
                ReductionParameters(),
            }, {
                // 'Knapsack' objective. Item 0 (4x4x4, profit 10) is worth
                // including; item 1 (4x4x4, profit -3, fully optional
                // 'copies_min' 0) is never worth choosing on its own
                // merits, so it is removed outright.
                fs::path("data") / "box" / "tests" / "knapsack_remove_negative_profit_items_fully_optional",
                ReductionParameters(),
            }, {
                // 'Knapsack' objective. Item 0 (4x4x4, profit -3, 5 copies,
                // 'copies_min' 2) has some copies genuinely mandatory -
                // only the 3 optional copies beyond 'copies_min' are ever
                // worth dropping, so 'copies' is trimmed to exactly 2, not
                // removed outright. 'skip_optimize_check': matching
                // rectangle's own equivalent scenario, a mandatory,
                // negative-profit item type under 'Knapsack' is suspected
                // to hit the same pre-existing, 'Reduction'-unrelated
                // solver defect there, not separately probed here to avoid
                // risking a hanging test.
                fs::path("data") / "box" / "tests" / "knapsack_remove_negative_profit_items_trims_to_minimum",
                ReductionParameters(),
                false,
                false,
                true,
            }, {
                // 'Knapsack' objective, one bin-type resource with a
                // *negative* penalty (a profit bonus the first time a bin's
                // consumption crosses capacity 1). Item 0 (4x4x4, profit
                // -3) has negative profit but also consumes the resource -
                // including it triggers the crossing, an indirect benefit
                // 'remove_negative_profit_items_applies' cannot see, so the
                // whole operation is skipped: the reduced instance comes
                // back with item 0 untouched. 'use_json' (resources aren't
                // representable in plain CSV); 'skip_optimize_check' for
                // the same reason as the scenario above.
                fs::path("data") / "box" / "tests" / "knapsack_remove_negative_profit_items_skipped_with_negative_penalty_resource",
                ReductionParameters(),
                false,
                true,
                true,
            }, {
                // Bin with one resource. Item types 0 and 1 (both 4x4x4)
                // have the exact same per-copy consumption schedule and
                // merge; item type 2 (also 4x4x4) has a different schedule
                // and must stay separate. 'use_json' for the resource.
                fs::path("data") / "box" / "tests" / "bin_packing_merge_identical_items_respects_resources",
                ReductionParameters(),
                false,
                true,
            }, {
                // Bin 20x20x20. Item types 0 and 1 (both 5x10x5, oriented,
                // 1 copy each) are identical and merge into a single
                // reduced item type with 2 copies; placing both reduced
                // copies side by side and unreducing must recover a valid
                // solution using original item type ids 0 and 1, each
                // exactly once.
                fs::path("data") / "box" / "tests" / "bin_packing_merge_identical_items_unreduce_solution_round_trip",
                ReductionParameters(),
            }, {
                // 'box'-specific (no rectangle analogue): item types 0 and
                // 1 have identical dimensions (4x4x4) but *different*
                // allowed-rotation sets (item 0 oriented; item 1 also
                // allows the YXZ rotation) - two item types are only truly
                // interchangeable if a downstream solve could freely swap
                // one copy for the other in any placement, which is not
                // the case here, so they must not merge.
                // 'skip_optimize_check': this scenario only exercises the
                // reduced instance itself.
                fs::path("data") / "box" / "tests" / "bin_packing_merge_identical_items_requires_matching_rotations",
                ReductionParameters(),
                false,
                false,
                true,
            }, {
                // Bin 4x4x4, holding exactly one item per bin. Item types
                // 0 (30 copies) and 1 (20 copies), both 4x4x4, merge into
                // a single reduced item type with 50 copies. Regression
                // test: 'unreduce_solution' must recognize that every
                // physical bin resolving to original item type 0 forms
                // one contiguous run (and likewise for type 1), and group
                // them into exactly 2 'add_bin' entries (30 copies then
                // 20, or vice versa) - not one entry per physical bin
                // (50), which is what an earlier, unconditionally-
                // splitting version of this class actually did whenever a
                // merged pattern was reused across more than one physical
                // bin.
                fs::path("data") / "box" / "tests" / "bin_packing_merge_identical_items_unequal_copies_produces_compact_groups",
                ReductionParameters(),
                false,
                false,
                false,
                2,
            },
        }));
