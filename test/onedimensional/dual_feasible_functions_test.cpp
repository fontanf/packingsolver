#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/dual_feasible_functions.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
///////////////////// OneDimensionalDualFeasibleFunctionsBoundTest ///////////////
////////////////////////////////////////////////////////////////////////////////

struct OneDimensionalDualFeasibleFunctionsBoundTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_bin_packing_bound;
};

inline std::ostream& operator<<(std::ostream& os, const OneDimensionalDualFeasibleFunctionsBoundTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class OneDimensionalDualFeasibleFunctionsBoundTest: public testing::TestWithParam<OneDimensionalDualFeasibleFunctionsBoundTestParams> { };

TEST_P(OneDimensionalDualFeasibleFunctionsBoundTest, OneDimensionalDualFeasibleFunctionsBound)
{
    OneDimensionalDualFeasibleFunctionsBoundTestParams test_params = GetParam();
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
        OneDimensional,
        OneDimensionalDualFeasibleFunctionsBoundTest,
        testing::ValuesIn(std::vector<OneDimensionalDualFeasibleFunctionsBoundTestParams>{
            {
                // Bin of length 10, three copies of a length-6 item: only
                // one fits per bin (2 * 6 = 12 > 10), so 3 bins are needed,
                // but the trivial length-sum bound (ceil(18 / 10) = 2)
                // misses this. The DFF sweep, at breakpoint k = 6 with the
                // f_ccm_0 family, scales both the item (6 > capacity - k =
                // 4, so it maps to the full capacity 10) and the bin itself
                // to 10, yielding the tight bound ceil(3 * 10 / 10) = 3.
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_item_length_over_half_capacity" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_item_length_over_half_capacity" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "bin_packing_item_length_over_half_capacity" / "parameters.csv",
                3,
            }}));

////////////////////////////////////////////////////////////////////////////////
///////////////// OneDimensionalDualFeasibleFunctionsInfeasibilityTest ///////////
////////////////////////////////////////////////////////////////////////////////

struct OneDimensionalDualFeasibleFunctionsInfeasibilityTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    bool expected_is_proven_infeasible;
};

inline std::ostream& operator<<(std::ostream& os, const OneDimensionalDualFeasibleFunctionsInfeasibilityTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class OneDimensionalDualFeasibleFunctionsInfeasibilityTest: public testing::TestWithParam<OneDimensionalDualFeasibleFunctionsInfeasibilityTestParams> { };

TEST_P(OneDimensionalDualFeasibleFunctionsInfeasibilityTest, OneDimensionalDualFeasibleFunctionsInfeasibility)
{
    OneDimensionalDualFeasibleFunctionsInfeasibilityTestParams test_params = GetParam();
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
        OneDimensional,
        OneDimensionalDualFeasibleFunctionsInfeasibilityTest,
        testing::ValuesIn(std::vector<OneDimensionalDualFeasibleFunctionsInfeasibilityTestParams>{
            {
                // Same length-10 bin / length-6 item shape as the bound
                // test above (bound 3), but only 2 bins available: proven
                // infeasible.
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_length_over_half_capacity_too_few_bins" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_length_over_half_capacity_too_few_bins" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_length_over_half_capacity_too_few_bins" / "parameters.csv",
                true,
            }, {
                // Same bin/item shape, but only 2 copies of the item and 2
                // bins available (bound 2, matching the number of bins):
                // genuinely feasible, must not be flagged.
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_length_over_half_capacity_enough_bins" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_length_over_half_capacity_enough_bins" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "feasibility_item_length_over_half_capacity_enough_bins" / "parameters.csv",
                false,
            }}));

////////////////////////////////////////////////////////////////////////////////
///////////////// OneDimensionalDualFeasibleFunctionsKnapsackBoundTest ///////////
////////////////////////////////////////////////////////////////////////////////

struct OneDimensionalDualFeasibleFunctionsKnapsackBoundTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    Profit expected_knapsack_bound;
};

inline std::ostream& operator<<(std::ostream& os, const OneDimensionalDualFeasibleFunctionsKnapsackBoundTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class OneDimensionalDualFeasibleFunctionsKnapsackBoundTest: public testing::TestWithParam<OneDimensionalDualFeasibleFunctionsKnapsackBoundTestParams> { };

TEST_P(OneDimensionalDualFeasibleFunctionsKnapsackBoundTest, OneDimensionalDualFeasibleFunctionsKnapsackBound)
{
    OneDimensionalDualFeasibleFunctionsKnapsackBoundTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    DualFeasibleFunctionsParameters dff_parameters;
    dff_parameters.verbosity_level = 0;
    DualFeasibleFunctionsOutput dff_output = dual_feasible_functions(instance, dff_parameters);

    EXPECT_NEAR(dff_output.knapsack_bound, test_params.expected_knapsack_bound, 1e-6);
}

INSTANTIATE_TEST_SUITE_P(
        OneDimensional,
        OneDimensionalDualFeasibleFunctionsKnapsackBoundTest,
        testing::ValuesIn(std::vector<OneDimensionalDualFeasibleFunctionsKnapsackBoundTestParams>{
            {
                // Bin of length 10, two copies of a length-6, profit-5 item:
                // only one fits (2 * 6 = 12 > 10), so the true optimal
                // profit is 5. The plain 1D continuous relaxation (pool all
                // 12 units of length against capacity 10, take the last
                // fractionally) would only bound this at 5 + 5 * (10 - 6) /
                // 6 ~ 8.33. The DFF sweep, at breakpoint k = 6 with the
                // f_ccm_0 family, scales the item and the bin the same way
                // as the bin packing bound test above (both to 10), so the
                // knapsack capacity becomes exactly one scaled item's worth
                // and the bound comes out tight at 5.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_item_length_over_half_capacity" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_item_length_over_half_capacity" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_item_length_over_half_capacity" / "parameters.csv",
                5.0,
            }}));
