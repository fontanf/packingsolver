#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/dual_feasible_functions.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
////////////////////// RectangleDualFeasibleFunctionsBoundTest ///////////////////
////////////////////////////////////////////////////////////////////////////////

struct RectangleDualFeasibleFunctionsBoundTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_bin_packing_bound;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleDualFeasibleFunctionsBoundTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleDualFeasibleFunctionsBoundTest: public testing::TestWithParam<RectangleDualFeasibleFunctionsBoundTestParams> { };

TEST_P(RectangleDualFeasibleFunctionsBoundTest, RectangleDualFeasibleFunctionsBound)
{
    RectangleDualFeasibleFunctionsBoundTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    DualFeasibleFunctionsParameters dff_parameters;
    dff_parameters.verbosity_level = 0;
    DualFeasibleFunctionsOutput dff_output = dual_feasible_functions(instance, dff_parameters);

    EXPECT_EQ(dff_output.bin_packing_bound, test_params.expected_bin_packing_bound);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleDualFeasibleFunctionsBoundTest,
        testing::ValuesIn(std::vector<RectangleDualFeasibleFunctionsBoundTestParams>{
            {
                // Bin 24400x12200 (non-square), one non-oriented item
                // 16200x820: trivially fits a single bin unrotated
                // (16200 <= 24400, 820 <= 12200). The item's width (16200)
                // exceeds the bin's *height* (12200) - since the item is
                // non-oriented, that width also feeds the height breakpoint
                // table (it could end up presenting either side along
                // either axis), and folding it via 'bin_height - 16200'
                // used to underflow to a negative breakpoint, corrupting
                // the CCM sweep and wrongly inflating the bound past 1.
                // Matches the dimensions of the real-world instance that
                // first surfaced this bug.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_oriented_item_wider_than_bin_height" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_oriented_item_wider_than_bin_height" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_oriented_item_wider_than_bin_height" / "parameters.csv",
                1,
            }, {
                // Bin 200x100 (non-square). Four copies of a non-oriented
                // 55x70 item: both dimensions exceed half the bin's height
                // (50) regardless of rotation, so only two fit per bin (side
                // by side, rotated), and four copies need exactly two bins -
                // but only via the rotation-aware breakpoint sweep, not via
                // the Clautiaux-doubling fallback alone, which pays a
                // "squaring tax" on this non-square bin (it inflates the
                // height capacity from 100 to 200, which is enough to hide
                // this particular violation).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_square_bin_rotatable_items" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_square_bin_rotatable_items" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_square_bin_rotatable_items" / "parameters.csv",
                2,
            }, {
                // Same bin and item as above, but only 2 copies: they fit
                // side by side along the width axis (2 * 70 = 140 <= 200,
                // height 55 <= 100) - a single bin genuinely suffices, must
                // not be over-estimated.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_square_bin_two_rotatable_items_fit_side_by_side" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_square_bin_two_rotatable_items_fit_side_by_side" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_non_square_bin_two_rotatable_items_fit_side_by_side" / "parameters.csv",
                1,
            }, {
                // Regression check: the original (pre-rotation-aware)
                // oriented-only case still works. Bin 100x100, two oriented
                // 60x60 items: both dimensions exceed half the bin in both
                // axes, so at most one fits per bin.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_square_bin_two_oriented_items_too_big" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_square_bin_two_oriented_items_too_big" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_square_bin_two_oriented_items_too_big" / "parameters.csv",
                2,
            }}));

////////////////////////////////////////////////////////////////////////////////
/////////////////////// RectangleDualFeasibleFunctionsCutTest ////////////////////
////////////////////////////////////////////////////////////////////////////////

struct RectangleDualFeasibleFunctionsCutTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    /** Whether a violated cut is expected to be found for the full item selection. */
    bool expect_violation;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleDualFeasibleFunctionsCutTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleDualFeasibleFunctionsCutTest: public testing::TestWithParam<RectangleDualFeasibleFunctionsCutTestParams> { };

TEST_P(RectangleDualFeasibleFunctionsCutTest, RectangleDualFeasibleFunctionsCut)
{
    // find_most_violated_dual_feasible_function_cut() is the checker meant
    // to run before building a Benders subproblem: the returned cut must be
    // valid for the whole instance's item universe, not just the exact
    // candidate that revealed it - so it is exercised here directly on the
    // full item selection.
    RectangleDualFeasibleFunctionsCutTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        selected_items.push_back(
                {item_type_id, instance.item_type(item_type_id).copies});
    }
    DualFeasibleFunctionsCut cut = find_most_violated_dual_feasible_function_cut(
            instance, 0, selected_items);

    if (test_params.expect_violation) {
        EXPECT_TRUE(cut.found);
        EXPECT_GT(cut.violation, 0.0);
        ASSERT_EQ((ItemTypeId)cut.coefficients.size(), instance.number_of_item_types());
        for (const std::pair<ItemTypeId, ItemPos>& selected_item: selected_items)
            EXPECT_GT(cut.coefficients[selected_item.first], 0.0);
    } else {
        EXPECT_FALSE(cut.found);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleDualFeasibleFunctionsCutTest,
        testing::ValuesIn(std::vector<RectangleDualFeasibleFunctionsCutTestParams>{
            {
                // Same instance as the non-square-bin bound test above with
                // 4 copies, exercised via the cut checker instead.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "parameters.csv",
                true,
            }, {
                // Same instance as the non-square-bin bound test above with
                // only 2 copies, exercised via the cut checker instead: no
                // violation should be found.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "parameters.csv",
                false,
            }}));
