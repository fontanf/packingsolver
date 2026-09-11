#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "packingsolver/onedimensional/reduction.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

/**
 * Structural comparison of every property 'Reduction' can affect, mirroring
 * 'rectangle::reduction_test.cpp''s own 'expect_instances_equal' - adapted
 * to this problem type's own 'ItemType'/'BinType' fields (a plain 'length'
 * instead of a 2D rect, no 'oriented' flag).
 */
void expect_instances_equal(const Instance& actual, const Instance& expected)
{
    ASSERT_EQ(actual.number_of_bin_types(), expected.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < expected.number_of_bin_types();
            ++bin_type_id) {
        const BinType& actual_bin_type = actual.bin_type(bin_type_id);
        const BinType& expected_bin_type = expected.bin_type(bin_type_id);
        EXPECT_EQ(actual_bin_type.length, expected_bin_type.length);
        EXPECT_EQ(actual_bin_type.copies, expected_bin_type.copies);
        EXPECT_EQ(actual_bin_type.copies_min, expected_bin_type.copies_min);
    }

    ASSERT_EQ(actual.number_of_item_types(), expected.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < expected.number_of_item_types();
            ++item_type_id) {
        const ItemType& actual_item_type = actual.item_type(item_type_id);
        const ItemType& expected_item_type = expected.item_type(item_type_id);
        EXPECT_EQ(actual_item_type.length, expected_item_type.length);
        EXPECT_EQ(actual_item_type.copies, expected_item_type.copies);
        EXPECT_EQ(actual_item_type.copies_min, expected_item_type.copies_min);
        EXPECT_EQ(actual_item_type.profit, expected_item_type.profit);
    }
}

}

/**
 * One 'Reduction' scenario, read from 'data/onedimensional/tests/<name>/' -
 * mirrors 'rectangle::RectangleReductionTestParams', minus
 * 'expected_proven_infeasible' ('onedimensional::Reduction' never proves
 * infeasibility on its own, unlike 'rectangle::Reduction': this class only
 * ever trims or merges item types, never sets bins aside or shrinks bin
 * capacity).
 */
struct OneDimensionalReductionTestParams
{
    fs::path dir;
    ReductionParameters reduction_parameters;
    /**
     * Use 'instance.json'/'reduced_instance.json' instead of 'items.csv'/
     * 'bins.csv'/'reduced_items.csv'/'reduced_bins.csv' - needed for any
     * feature the CSV format cannot represent (resources, eligibility).
     */
    bool use_json = false;
    /**
     * Do not call 'optimize()' at all (so no 'solution.csv' is read or
     * needed): some scenarios exist specifically to exercise 'Reduction'
     * itself against a combination this codebase's solvers cannot
     * currently handle downstream - a pre-existing, 'Reduction'-unrelated
     * defect (a negative-penalty resource, or a mandatory 'copies_min' item
     * type with negative profit, both under 'Knapsack' - matching the same
     * two cases rectangle's own suite documents) that would otherwise hang
     * or wrongly report infeasible. Only 'expect_instances_equal' runs; the
     * reduced instance is still checked as usual.
     */
    bool skip_optimize_check = false;
    /**
     * When non-negative, the exact 'Solution::number_of_different_bins()'
     * the unreduced solution must have - catches 'unreduce_solution'
     * exploding a pattern shared by many physical bins into one entry per
     * physical copy instead of grouping consecutive copies that resolved
     * to the same original item ids (a real regression this class once
     * had: a merge of two very unequal-copies item types used to produce
     * as many distinct bins as there were physical copies, instead of
     * one per original item type actually involved).
     */
    BinPos expected_number_of_different_bins = -1;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const OneDimensionalReductionTestParams& test_params)
{
    os << test_params.dir;
    return os;
}

class OneDimensionalReductionTest: public testing::TestWithParam<OneDimensionalReductionTestParams> { };

TEST_P(OneDimensionalReductionTest, OneDimensionalReduction)
{
    OneDimensionalReductionTestParams test_params = GetParam();

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
    onedimensional::Output output = optimize(instance, optimize_parameters);

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
        OneDimensional,
        OneDimensionalReductionTest,
        testing::ValuesIn(std::vector<OneDimensionalReductionTestParams>{
            {
                // Bin length 100, 5 copies (BinPacking, so no wide/tall/
                // companion-style reduction exists at all for this problem
                // type to begin with - isolating 'merge_identical_items').
                // Two item types, both length 4, identical on every
                // property 'items_mergeable' checks: they merge into a
                // single item type with the combined copies.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items",
                ReductionParameters(),
            }, {
                // Same as above, but the two item types have different
                // profit - not compared by 'items_mergeable' for
                // 'BinPacking' (profit is never the actual objective here,
                // and 'unreduce_solution' always restores each placed
                // copy's own true original profit regardless of merging),
                // so these two still merge.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_different_profit_still_merges",
                ReductionParameters(),
            }, {
                // Bin length 100, 'Knapsack' objective. Item types 0 and 1
                // (both length 4, profit 5) are identical including profit
                // and merge; item type 2 (also length 4, profit 10) is
                // identical on every *other* property but must stay
                // separate - unlike every other objective this class
                // handles, 'Knapsack' optimizes profit directly, so merging
                // different-profit item types would report one uniform
                // profit for every copy and let the solve choose a
                // suboptimal subset on wrong information.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_merge_identical_items_requires_matching_profit",
                ReductionParameters(),
            }, {
                // Bin length 100, 'Knapsack' objective. Item 0 (length 4,
                // profit 10) is worth including; item 1 (length 4, profit
                // -3, fully optional 'copies_min' 0) is never worth
                // choosing on its own merits, so it is removed outright.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_remove_negative_profit_items_fully_optional",
                ReductionParameters(),
            }, {
                // Bin length 100, 'Knapsack' objective. Item 0 (length 4,
                // profit -3, 5 copies, 'copies_min' 2) has some copies
                // genuinely mandatory - only the 3 optional copies beyond
                // 'copies_min' are ever worth dropping, so 'copies' is
                // trimmed to exactly 2, not removed outright.
                // 'skip_optimize_check': confirmed separately (both with
                // and without '--reduce') that a mandatory, negative-profit
                // item type under 'Knapsack' makes this codebase's solver
                // wrongly report the instance as proven infeasible - the
                // same pre-existing, 'Reduction'-unrelated defect class
                // rectangle's own suite documents (there it hangs instead
                // of misreporting; either way, not this class's own
                // mistake to fix), well outside this class's own scope.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_remove_negative_profit_items_trims_to_minimum",
                ReductionParameters(),
                false,
                true,
            }, {
                // Bin length 100, 'Knapsack' objective, one resource with a
                // *negative* penalty (a profit bonus the first time a bin's
                // consumption crosses capacity 1). Item 0 (length 4, profit
                // -3) has negative profit but also consumes the resource -
                // including it triggers the crossing (bonus 100), making it
                // worth including despite its own negative profit, an
                // indirect benefit 'remove_negative_profit_items_applies'
                // cannot see, so the whole operation is skipped.
                // 'skip_optimize_check': confirmed separately (both with
                // and without '--reduce') that this codebase's solver hangs
                // on this negative-penalty-resource / 'Knapsack'
                // combination - a pre-existing bug unrelated to
                // 'Reduction', matching rectangle's own documented case.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_remove_negative_profit_items_skipped_with_negative_penalty_resource",
                ReductionParameters(),
                true,
                true,
            }, {
                // 'VariableSizedBinPacking' (not 'BinPacking': its bin
                // types must be used in the exact order they are declared,
                // which would force every copy of the first bin type to be
                // opened - empty or not - before the second, eligibility-
                // restricted one could ever be reached, entangling this
                // case with a constraint that has nothing to do with what
                // it means to test). Two bin types: bin type 0 accepts any
                // item (no eligibility restriction); bin type 1 only
                // accepts items with eligibility id 1. Item type 0 (length
                // 4, eligibility -1, "any bin") and item type 1 (length 4,
                // eligibility 1) are identical on every other property, but
                // 'onedimensional::ItemType' has no 'group_id' the way
                // rectangle's does - 'eligibility_id' is this type's own
                // analogous "must not silently swap" property - so they
                // must not merge.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_requires_matching_eligibility",
                ReductionParameters(),
                true,
            }, {
                // Bin length 100 with one resource. Item types 0 and 1
                // (both length 4) have the exact same per-copy consumption
                // schedule and merge; item type 2 (also length 4) has a
                // different schedule and must stay separate.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_respects_resources",
                ReductionParameters(),
                true,
            }, {
                // Bin length 20 (exactly two items' worth, unlike
                // rectangle's 2D case there is no companion/full-span
                // mechanism here to entangle with). Item types 0 and 1
                // (both length 10, 1 copy each) are identical and merge
                // into a single reduced item type with 2 copies; placing
                // both reduced copies side by side and unreducing must
                // recover a valid solution using original item type ids 0
                // and 1, each exactly once.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_unreduce_solution_round_trip",
                ReductionParameters(),
            }, {
                // Bin length 4, holding exactly one item per bin. Item
                // types 0 (30 copies) and 1 (20 copies), both length 4,
                // merge into a single reduced item type with 50 copies.
                // Regression test: 'unreduce_solution' must recognize that
                // every physical bin resolving to original item type 0
                // forms one contiguous run (and likewise for type 1), and
                // group them into exactly 2 'add_bin' entries (30 copies
                // then 20, or vice versa) - not one entry per physical
                // bin (50), which is what an earlier, unconditionally-
                // splitting version of this class actually did whenever a
                // merged pattern was reused across more than one physical
                // bin.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_unequal_copies_produces_compact_groups",
                ReductionParameters(),
                false,
                false,
                2,
            },
        }));
