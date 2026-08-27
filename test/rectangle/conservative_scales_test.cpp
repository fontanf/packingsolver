#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "packingsolver/rectangle/reduction.hpp"
#include "rectangle/conservative_scales.hpp"
#include "rectangle/dual_feasible_functions.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(RectangleConservativeScales, NonOrientedItemThrows)
{
    // A non-oriented item can rotate, so treating its stored (width, height)
    // as fixed - as the rescaling logic does - could derive an unsound
    // bound (see Instance::all_item_types_oriented's doc comment). Callers must check
    // this first; conservative_scales itself must refuse to run instead of
    // silently computing something invalid.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, -1);
    ItemTypeId item_type_id = instance_builder.add_item_type(8, 3, false);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    EXPECT_FALSE(instance.all_item_types_oriented());

    ConservativeScalesParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(conservative_scales(instance, parameters), std::invalid_argument);
}

struct RectangleConservativeScalesBoundTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_bin_packing_bound;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleConservativeScalesBoundTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleConservativeScalesBoundTest: public testing::TestWithParam<RectangleConservativeScalesBoundTestParams> { };

TEST_P(RectangleConservativeScalesBoundTest, RectangleConservativeScalesBound)
{
    RectangleConservativeScalesBoundTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    ConservativeScalesParameters parameters;
    parameters.verbosity_level = 0;
    ConservativeScalesOutput output = conservative_scales(instance, parameters);

    EXPECT_EQ(output.bin_packing_bound, test_params.expected_bin_packing_bound);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleConservativeScalesBoundTest,
        testing::ValuesIn(std::vector<RectangleConservativeScalesBoundTestParams>{
            {
                // Bin 10x10, one item 5x5 (area 25): the bound should be
                // exactly 1, the trivial area ceiling, with no rescaling
                // needed.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_single_oriented_item_area_bound" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_single_oriented_item_area_bound" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_single_oriented_item_area_bound" / "parameters.csv",
                1,
            }, {
                // Bin 10x10, two 6x6 items: total area 72 <= 100 (naive
                // area bound is only 1), but two 6-wide items can't sit
                // side by side (6+6=12 > 10) nor stacked (same on height),
                // so the true answer is 2 bins. Each item's own
                // width/height (6) already exceeds half the bin (5), so
                // this also happens to be provable by the existing
                // per-axis dual feasible functions bound - this case
                // exercises that the new rescaling + knapsack-separation +
                // (k,l) combination pipeline reaches the same correct,
                // non-trivial answer end to end.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_oriented_items_neither_side_by_side_nor_stacked" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_oriented_items_neither_side_by_side_nor_stacked" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_oriented_items_neither_side_by_side_nor_stacked" / "parameters.csv",
                2,
            }}));

TEST(RectangleConservativeScales, Class01Size40Instance1TightensBoundToTen)
{
    // The motivating real-world case: Class_01.2bp_40_1 from the berkey1987
    // benchmark set. The existing dual-feasible-functions bound plateaus at
    // 9 here (a manual first-fit-decreasing packing of the 40 item *areas*
    // into capacity-100 bins already achieves exactly 9, so no purely
    // per-axis/area argument can ever prove more), while a known-feasible
    // packing uses 10 bins - this is exactly the instance that motivated
    // implementing L_BKRS, and it does close the gap: with this bound in
    // place, plain tree search now proves the instance optimal (10 bins,
    // 0% gap) without ever needing Benders decomposition.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.read_item_types("data/rectangle/berkey1987/Class_01.2bp_40_1_items.csv");
    instance_builder.read_bin_types("data/rectangle/berkey1987/Class_01.2bp_40_1_bins.csv");
    instance_builder.set_bin_type_copies(0, -1);
    instance_builder.set_item_types_oriented();
    Instance instance = instance_builder.build();

    DualFeasibleFunctionsParameters dff_parameters;
    dff_parameters.verbosity_level = 0;
    DualFeasibleFunctionsOutput dff_output = dual_feasible_functions(instance, dff_parameters);

    ConservativeScalesParameters cs_parameters;
    cs_parameters.verbosity_level = 0;
    ConservativeScalesOutput cs_output = conservative_scales(instance, cs_parameters);

    EXPECT_EQ(dff_output.bin_packing_bound, 9);
    EXPECT_EQ(cs_output.bin_packing_bound, 10);
}

TEST(RectangleConservativeScales, BoundUnaffectedByMergingIdenticalItems)
{
    // Class_01.2bp_60_7 from the berkey1987 benchmark set: merging its
    // identical item types (see 'Reduction::merge_identical_items') used to
    // weaken this bound from 16 down to 15 - 'solve_conservative_scale_lp'
    // had one rescaling variable per *item type*, so two physical items
    // merged into a single type with 'copies' 2 were forced to share one
    // rescaled value instead of being free to rescale independently, losing
    // exactly the flexibility needed to reach 16. The LP is now built with
    // one variable per physical item copy (see that function's own doc
    // comment), so the bound no longer depends on whether the caller's
    // instance happens to have already merged identical item types
    // together.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.read_item_types("data/rectangle/berkey1987/Class_01.2bp_60_7_items.csv");
    instance_builder.read_bin_types("data/rectangle/berkey1987/Class_01.2bp_60_7_bins.csv");
    instance_builder.set_bin_type_copies(0, -1);
    instance_builder.set_item_types_oriented();
    Instance instance = instance_builder.build();

    // Every other reduction operation disabled, so 'merge_identical_items'
    // is the only difference between the two instances built below - a
    // change from any of the others (e.g. item enlargement) could shift
    // this bound for unrelated reasons and defeat the point of this test.
    ReductionParameters reduction_parameters;
    reduction_parameters.enlarge_wide_tall_items = false;
    reduction_parameters.enlarge_both_items = false;
    reduction_parameters.reduce_full_bin_items = false;
    reduction_parameters.reduce_perfect_pairs = false;
    reduction_parameters.remove_negative_profit_items = false;
    reduction_parameters.remove_dominated_items = false;
    reduction_parameters.remove_dominated_bin_types = false;
    reduction_parameters.merge_identical_items = false;
    Reduction reduction_unmerged(instance, reduction_parameters);
    reduction_parameters.merge_identical_items = true;
    Reduction reduction_merged(instance, reduction_parameters);
    // The merge must actually have happened, or this test is not exercising
    // what it claims to.
    ASSERT_LT(
            reduction_merged.instance().number_of_item_types(),
            reduction_unmerged.instance().number_of_item_types());

    ConservativeScalesParameters cs_parameters;
    cs_parameters.verbosity_level = 0;
    ConservativeScalesOutput unmerged_output = conservative_scales(reduction_unmerged.instance(), cs_parameters);
    ConservativeScalesOutput merged_output = conservative_scales(reduction_merged.instance(), cs_parameters);

    EXPECT_EQ(unmerged_output.bin_packing_bound, 16);
    EXPECT_EQ(merged_output.bin_packing_bound, 16);
}
