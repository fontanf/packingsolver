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

/**
 * Default parameters, minus the given operations - for scenarios isolating
 * one operation on an instance another one would otherwise already reduce.
 */
ReductionParameters parameters_without(
        const std::vector<bool ReductionParameters::*>& disabled_operations)
{
    ReductionParameters parameters;
    for (bool ReductionParameters::* operation: disabled_operations)
        parameters.*operation = false;
    return parameters;
}

}

/**
 * One 'Reduction' scenario, read from 'data/onedimensional/tests/<name>/' -
 * mirrors 'rectangle::RectangleReductionTestParams' (minus its
 * 'expected_output_infeasible'/'use_json': every scenario here is provably
 * infeasible by the reduction alone, if at all, since a finite single bin
 * type's own copies is the only way 'BinPacking' can be infeasible here,
 * and that is always exactly what 'reduce_full_bin_items'/
 * 'reduce_perfect_pairs' - the only operations that ever consume bin
 * copies - already check for themselves).
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
    /** 'Reduction::proven_infeasible()' alone proves the instance infeasible. */
    bool expected_proven_infeasible = false;
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
    EXPECT_EQ(reduction.proven_infeasible(), test_params.expected_proven_infeasible);
    if (test_params.expected_proven_infeasible) {
        OptimizeParameters optimize_parameters;
        optimize_parameters.reduction_parameters = test_params.reduction_parameters;
        onedimensional::Output output = optimize(instance, optimize_parameters);
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
                parameters_without({&ReductionParameters::reduce_dominant_sets}),
            }, {
                // Same as above, but the two item types have different
                // profit - not compared by 'items_mergeable' for
                // 'BinPacking' (profit is never the actual objective here,
                // and 'unreduce_solution' always restores each placed
                // copy's own true original profit regardless of merging),
                // so these two still merge.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_different_profit_still_merges",
                parameters_without({&ReductionParameters::reduce_dominant_sets}),
            }, {
                // Bin length 100, 2 copies, 'Knapsack' objective (2 bins,
                // so not a classical knapsack, for which merging is skipped
                // - see 'knapsack_single_bin_only_removes_negative_profit_items').
                // Item types 0 and 1
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
                // Bin length 100, 1 copy, 'Knapsack' objective, no other
                // constraint: a classical knapsack, which only gets
                // 'remove_negative_profit_items'.
                // Item type 2 (profit -3, fully optional) is removed, but
                // item types 0 and 1 (both length 4, profit 5) are not
                // merged, although they are identical.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_single_bin_only_removes_negative_profit_items",
                ReductionParameters(),
            }, {
                // Bin length 100, 1 copy, 'Knapsack' objective, but every
                // item type has a 'maximum_stackability' of 3, which
                // 'knapsacksolver::dynamic_programming_primal_dual' ignores:
                // not a classical knapsack, so item types 0 and 1 (both
                // length 4, profit 5) are still merged. At most 3 items fit
                // in the bin (profit 15).
                fs::path("data") / "onedimensional" / "tests" / "knapsack_single_bin_with_side_constraint_still_merges",
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
                parameters_without({&ReductionParameters::remove_dominated_bin_types}),
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
                // Bin length 20 (exactly two items' worth). Item types 0
                // and 1 (both length 10, 1 copy each) are identical and
                // would merge into a single reduced item type with 2
                // copies - but 'reduce_perfect_pairs' (which now runs
                // ahead of 'merge_identical_items' - see this class's own
                // doc comment in 'reduction.hpp') claims them first as a
                // dedicated bin instead, leaving an empty reduced
                // instance: unreducing that dedicated-bin reservation must
                // still recover a valid solution using original item type
                // ids 0 and 1, each exactly once.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_unreduce_solution_round_trip",
                ReductionParameters(),
            }, {
                // Bin length 4, holding exactly one item per bin. Item
                // types 0 (30 copies) and 1 (20 copies), both length 4,
                // would merge into a single reduced item type with 50
                // copies - but 'reduce_full_bin_items' (see the comment
                // above) claims every copy of both as its own dedicated
                // bin first, leaving an empty reduced instance.
                // Regression test: 'unreduce_solution' must recognize that
                // every physical bin resolving to original item type 0
                // forms one contiguous run (and likewise for type 1), and
                // group them into exactly 2 'add_bin' entries (30 copies
                // then 20, or vice versa) - not one entry per physical
                // bin (50), which is what an earlier, unconditionally-
                // splitting version of this class actually did whenever a
                // reused pattern was shared across more than one physical
                // bin.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_merge_identical_items_unequal_copies_produces_compact_groups",
                ReductionParameters(),
                false,
                false,
                2,
            }, {
                // Bin length 10, copies 5 (BinPacking, single bin type).
                // Item type 0 (length 10, 3 copies) exactly matches the
                // bin's own length: 'reduce_full_bin_items' reserves all 3
                // copies as their own dedicated bins, leaving an empty
                // reduced instance and folding the bin type's own copies
                // down to 5 - 3 = 2.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_full_bin_item",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 6. Item types 0 (length 6, 4
                // copies) and 1 (length 4, 4 copies) sum exactly to the
                // bin's own length: 'reduce_perfect_pairs' reserves
                // min(4, 4) = 4 dedicated bins, consuming every copy of
                // both, leaving an empty reduced instance and folding the
                // bin type's own copies down to 6 - 4 = 2.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_perfect_pair",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 10. Item type 0 (length 5, 6
                // copies): 2 of its own copies already sum exactly to the
                // bin's own length, so 'reduce_perfect_pairs' pairs it
                // with itself, reserving 6 / 2 = 3 dedicated bins (two
                // copies each), consuming every copy and leaving an empty
                // reduced instance, folding the bin type's own copies down
                // to 10 - 3 = 7.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_perfect_pair_same_item_type",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 2 (finite). Item type 0 (length
                // 10, 2 copies) exactly matches the bin's own length:
                // 'reduce_full_bin_items' reserves both copies as their
                // own dedicated bins, exhausting the bin type's entire
                // (finite) 'copies' - but item type 1 (length 3, 5 copies)
                // still needs bin capacity that no longer exists, so the
                // reduction alone already proves the instance infeasible
                // (see 'Reduction::proven_infeasible').
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_full_bin_item_proven_infeasible",
                ReductionParameters(),
                false,
                false,
                -1,
                true,
            }, {
                // Bin length 10, copies 5. Item type 0 (length 6, 3
                // copies), item type 1 (length 3, 1 copy), item type 2
                // (length 1, 1 copy). 'reduce_dominant_sets', rule 0: the
                // first copy of item type 0 leaves 10 - 6 = 4 beside it,
                // and everything fitting there (3 + 1 = 4) fits together,
                // so they share a dedicated bin. The other two copies of
                // item type 0 then have nothing left to fit beside them, so
                // each gets its own dedicated bin - both grouped into a
                // single two-copy bin record. Everything is reserved,
                // folding the bin type's own copies down to 5 - 3 = 2.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_dominant_set_all_fit",
                ReductionParameters(),
                false,
                false,
                2,
            }, {
                // Bin length 20, copies 5. Item type 0 (length 6, 1 copy)
                // is the only item type with exactly one remaining copy:
                // 'lift_item_lengths' checks whether item type 1 (length 3,
                // 2 copies) could ever reach into its own leftover space
                // (20 - 6 = 14) - the largest achievable combination of
                // item type 1's own copies is 3 + 3 = 6, strictly less
                // than 14, so the remaining 14 - 6 = 8 is provably always
                // wasted regardless of arrangement: item type 0's own
                // declared length is grown to 20 - 6 = 14 in the reduced
                // instance. Item type 1 is untouched (2 copies, not
                // eligible on its own). The reduced instance's own optimal
                // solve (14 + 3 + 3 = 20, an exact fit) matches the
                // original instance's own true optimal (6 + 3 + 3 = 12,
                // leaving 8 waste) once 'unreduce_solution' restores item
                // type 0's true length.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_lift_item_length",
                parameters_without({
                        &ReductionParameters::reduce_dominant_sets,
                        &ReductionParameters::shrink_bin}),
            }, {
                // Bin length 20, copies 5. Item type 0 (length 6, 1 copy)
                // leaves 20 - 6 = 14 beside it. Item type 1 (length 10,
                // nesting length 3, 2 copies) only takes 10 - 3 = 7 once
                // it follows another item, so both of its copies do reach
                // exactly 14 (6 + 7 + 7 = 20): 'lift_item_lengths' must
                // leave item type 0 untouched, and the reduced instance is
                // the original one.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_lift_item_length_nesting",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 5. Item type 0 (length 7, 1 copy),
                // item type 1 (length 2, 7 copies). 'reduce_dominant_sets',
                // rule 1: beside item type 0 (10 - 7 = 3), no two copies of
                // item type 1 fit together (2 + 2 > 3), so one copy of it
                // dominates and they share a dedicated bin. The remaining 6
                // copies of item type 1 match no rule (5 other copies do
                // not all fit beside one, and three of them fit together),
                // so they stay in the reduced instance, with 5 - 1 = 4 bin
                // copies left.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_dominant_set_single",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 5. Item type 0 (length 5, 1 copy),
                // item type 1 (length 3, 2 copies), item type 2 (length 2,
                // 1 copy). 'reduce_dominant_sets', rule 2, for item type 0
                // (5 left beside it): 'a' = 3, 'b' = 2 (the longest other
                // copy fitting beside both), no three copies fit together
                // (2 + 3 + 3 > 5), and no two copies longer than 'b' do
                // either (3 + 3 > 5), so '{3, 2}' dominates and they share
                // a dedicated bin. The last copy of item type 1 then has
                // nothing left beside it (rule 0) and gets its own
                // dedicated bin, folding the bin type's own copies down to
                // 5 - 2 = 3.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_dominant_set_pair",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 10. Item types 0 (length 6) and 1
                // (length 4) form a perfect pair, but item type 2 (length
                // 7, nesting length 3, 2 copies) only takes 4 when it
                // follows another item: reserving '{6, 4}' would leave the
                // two copies of item type 2 needing a bin each (7 + 4 >
                // 10), 3 bins in total, while '{6, 7}' and '{4, 7}' only
                // need 2. 'full_bin_reduction_applies' is 'false' (an item
                // type has a nesting length), so nothing is reserved and
                // the reduced instance is the original one.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_perfect_pair_nesting_elsewhere",
                ReductionParameters(),
            }, {
                // Bin length 10, copies 5. Item type 0 (length 4, 2
                // copies). The largest combination of the items' lengths
                // is 4 + 4 = 8, so the bin shrinks to 8, and the two copies
                // form a perfect pair of the shrunk bin: 'reduce_perfect_pairs'
                // reserves them a dedicated bin, folding the bin type's own
                // copies down to 5 - 1 = 4. ('reduce_dominant_sets', which
                // would find the same bin on its own, is disabled.)
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_shrink_bin_perfect_pair",
                parameters_without({&ReductionParameters::reduce_dominant_sets}),
            }, {
                // 'VariableSizedBinPacking'. Bin type 0 (length 10, cost 2,
                // 10 copies) dominates bin type 1 (length 8, cost 3): at
                // least as long, at most as costly, and enough copies (10)
                // to replace every bin a solution could use (2 items).
                // Bin type 2 (length 12, cost 3) is longer than bin type 0,
                // and bin type 0 is cheaper than bin type 2, so neither
                // dominates the other. Bin type 1 is left out of the
                // reduced instance. The optimal solution puts both items
                // (length 6, 2 copies) in one bin of type 2 (cost 3, vs 4
                // for two bins of type 0) - bin type 1 in the reduced
                // instance, which 'unreduce_solution' maps back to 2.
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_remove_dominated_bin_types",
                ReductionParameters(),
            },
        }));
