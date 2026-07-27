#include "packingsolver/box/instance_builder.hpp"
#include "box/dual_feasible_functions.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::box;
namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
////////////////////// BoxDualFeasibleFunctionsInfeasibilityTest /////////////////
////////////////////////////////////////////////////////////////////////////////

struct BoxDualFeasibleFunctionsInfeasibilityTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    bool expected_is_proven_infeasible;
};

inline std::ostream& operator<<(std::ostream& os, const BoxDualFeasibleFunctionsInfeasibilityTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxDualFeasibleFunctionsInfeasibilityTest: public testing::TestWithParam<BoxDualFeasibleFunctionsInfeasibilityTestParams> { };

TEST_P(BoxDualFeasibleFunctionsInfeasibilityTest, BoxDualFeasibleFunctionsInfeasibility)
{
    BoxDualFeasibleFunctionsInfeasibilityTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    DualFeasibleFunctionsParameters dff_parameters;
    dff_parameters.verbosity_level = 0;
    DualFeasibleFunctionsOutput dff_output = dual_feasible_functions(instance, dff_parameters);

    EXPECT_EQ(dff_output.is_proven_infeasible, test_params.expected_is_proven_infeasible);
}

INSTANTIATE_TEST_SUITE_P(
        Box,
        BoxDualFeasibleFunctionsInfeasibilityTest,
        testing::ValuesIn(std::vector<BoxDualFeasibleFunctionsInfeasibilityTestParams>{
            {
                // Bin 200x100x100. Four copies of an item 55x70x100, allowed
                // to rotate by swapping its x/y sides (only) - its z side
                // always matches the bin's z exactly, so this reduces to the
                // same 2D case validated for rectangle (55x70 unoriented in
                // a 200x100 bin): both x/y sides exceed half the bin's
                // y-height (50) regardless of rotation, so it is provably
                // infeasible for a single bin.
                fs::path("data") / "box" / "tests" / "infeasible_rotation_two_axis_swap" / "items.csv",
                fs::path("data") / "box" / "tests" / "infeasible_rotation_two_axis_swap" / "bins.csv",
                fs::path("data") / "box" / "tests" / "infeasible_rotation_two_axis_swap" / "parameters.csv",
                true,
            }, {
                // Same bin and item as above, but only 2 copies: they fit
                // side by side along the bin's x-axis (2 * 70 = 140 <= 200,
                // 55 <= 100) - genuinely feasible, must not be flagged.
                fs::path("data") / "box" / "tests" / "feasible_rotation_two_axis_swap" / "items.csv",
                fs::path("data") / "box" / "tests" / "feasible_rotation_two_axis_swap" / "bins.csv",
                fs::path("data") / "box" / "tests" / "feasible_rotation_two_axis_swap" / "parameters.csv",
                false,
            }, {
                // Bin 100x100x100 (cube). Two copies of an oriented-looking
                // 60x60x60 cube item with *all 5 non-identity* rotations
                // allowed (rotation is a no-op for a cube, but exercises the
                // full rotation-enumeration code path): both dimensions
                // exceed half the bin in every axis, so at most one fits,
                // regardless of the (here, irrelevant) rotation freedom.
                fs::path("data") / "box" / "tests" / "infeasible_full_rotation_freedom" / "items.csv",
                fs::path("data") / "box" / "tests" / "infeasible_full_rotation_freedom" / "bins.csv",
                fs::path("data") / "box" / "tests" / "infeasible_full_rotation_freedom" / "parameters.csv",
                true,
            }, {
                // Regression check: the original (pre-rotation-aware)
                // fully-oriented case still works. Bin 100x100x100, two
                // oriented 60x60x60 items.
                fs::path("data") / "box" / "tests" / "infeasible_single_rotation_unchanged" / "items.csv",
                fs::path("data") / "box" / "tests" / "infeasible_single_rotation_unchanged" / "bins.csv",
                fs::path("data") / "box" / "tests" / "infeasible_single_rotation_unchanged" / "parameters.csv",
                true,
            }}));

////////////////////////////////////////////////////////////////////////////////
///////////////////////// BoxDualFeasibleFunctionsBoundTest //////////////////////
////////////////////////////////////////////////////////////////////////////////

struct BoxDualFeasibleFunctionsBoundTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_bin_packing_bound;
};

inline std::ostream& operator<<(std::ostream& os, const BoxDualFeasibleFunctionsBoundTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxDualFeasibleFunctionsBoundTest: public testing::TestWithParam<BoxDualFeasibleFunctionsBoundTestParams> { };

TEST_P(BoxDualFeasibleFunctionsBoundTest, BoxDualFeasibleFunctionsBound)
{
    BoxDualFeasibleFunctionsBoundTestParams test_params = GetParam();
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
        Box,
        BoxDualFeasibleFunctionsBoundTest,
        testing::ValuesIn(std::vector<BoxDualFeasibleFunctionsBoundTestParams>{
            {
                // Bin 24400x12200x100. One item 16200x820x100, allowed to
                // rotate by swapping x/y (in addition to staying unrotated)
                // - it trivially fits unrotated (16200<=24400, 820<=12200,
                // 100<=100). The *rotated* placement would need 16200 along
                // the bin's y-axis, which doesn't fit (16200 > 12200) - but
                // the breakpoint-table construction still folds this value
                // via 'bin_y - 16200' and collects it into the "big values"
                // used for f_ccm_1's cardinality bookkeeping, both of which
                // underflow negative if unguarded (mirrors the rectangle DFF
                // bug fixed on this branch, matching dimensions of the
                // real-world instance that first surfaced it). A single
                // unrotated item obviously fits in one bin: the bound must
                // stay at the honest area-ratio value of 1, not be inflated.
                fs::path("data") / "box" / "tests" / "bin_packing_rotated_dimension_exceeding_other_axis" / "items.csv",
                fs::path("data") / "box" / "tests" / "bin_packing_rotated_dimension_exceeding_other_axis" / "bins.csv",
                fs::path("data") / "box" / "tests" / "bin_packing_rotated_dimension_exceeding_other_axis" / "parameters.csv",
                1,
            }}));
