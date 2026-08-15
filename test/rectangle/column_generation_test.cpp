#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
///////////////////// RectangleColumnGenerationBinPackingTest ////////////////////
////////////////////////////////////////////////////////////////////////////////

struct RectangleColumnGenerationBinPackingTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_number_of_bins;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleColumnGenerationBinPackingTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleColumnGenerationBinPackingTest: public testing::TestWithParam<RectangleColumnGenerationBinPackingTestParams> { };

TEST_P(RectangleColumnGenerationBinPackingTest, RectangleColumnGenerationBinPacking)
{
    // Exercises 'column_generation''s sequential feasibility scheme (see
    // 'ColumnGenerationParameters::use_sequential_feasibility' in
    // 'algorithms/column_generation.hpp'), used by default for the
    // 'BinPacking' objective and required (regardless of the parameter) once
    // there is more than one bin type - column generation's usual bound
    // conversion (dividing the master's cost-minimizing LP bound by
    // bin_type(0).cost) is not valid there, so it can only be solved to
    // optimality this way.
    RectangleColumnGenerationBinPackingTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.use_tree_search = false;
    parameters.use_tree_search_maximal_spaces = false;
    parameters.use_sequential_value_correction = false;
    parameters.use_column_generation = true;
    parameters.use_dichotomic_search = false;
    parameters.use_sequential_single_knapsack = false;
    parameters.use_benders_decomposition = false;
    parameters.verbosity_level = 0;
    rectangle::Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), test_params.expected_number_of_bins);
    EXPECT_EQ(output.bin_packing_bound, test_params.expected_number_of_bins);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleColumnGenerationBinPackingTest,
        testing::ValuesIn(std::vector<RectangleColumnGenerationBinPackingTestParams>{
            {
                // Two bin types of different costs (50 and 80): column
                // generation's usual bound conversion is unsound here, so
                // this can only be solved to optimality via the sequential
                // feasibility scheme. Optimal solution uses 3 bins (2 of the
                // cheaper type, 1 of the costlier one).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "parameters.csv",
                3,
            }, {
                // Reused from 'benders_decomposition_test.cpp': two bin
                // types (same cost here, but still more than one, so this
                // also goes through the sequential feasibility scheme by
                // default), 3 copies of a single item type. Reference
                // solution (see its own 'solution.csv') uses 1 bin of the
                // first type and 2 of the second, 3 bins total.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "parameters.csv",
                3,
            }, {
                // Reused from 'benders_decomposition_test.cpp': a single bin
                // type but two different item sizes that must be mixed
                // within a bin to reach the optimum of 2 bins (see its own
                // 'solution.csv') - exercises column generation's regular
                // item-mix pricing rather than the sequential feasibility
                // scheme (only one bin type here).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "parameters.csv",
                2,
            }}));

////////////////////////////////////////////////////////////////////////////////
///////////////// RectangleColumnGenerationVariableSizedBinPackingTest ///////////
////////////////////////////////////////////////////////////////////////////////

struct RectangleColumnGenerationVariableSizedBinPackingTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    BinPos expected_number_of_bins;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleColumnGenerationVariableSizedBinPackingTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleColumnGenerationVariableSizedBinPackingTest: public testing::TestWithParam<RectangleColumnGenerationVariableSizedBinPackingTestParams> { };

TEST_P(RectangleColumnGenerationVariableSizedBinPackingTest, RectangleColumnGenerationVariableSizedBinPacking)
{
    // Regression check that the 'VariableSizedBinPacking' objective's own
    // bound conversion (which multiplies rather than divides by a bin cost -
    // see 'column_generation''s 'new_bound_callback') is unaffected by the
    // 'BinPacking'-only sequential feasibility scheme added alongside it.
    RectangleColumnGenerationVariableSizedBinPackingTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.use_tree_search = false;
    parameters.use_tree_search_maximal_spaces = false;
    parameters.use_sequential_value_correction = false;
    parameters.use_column_generation = true;
    parameters.use_dichotomic_search = false;
    parameters.use_sequential_single_knapsack = false;
    parameters.use_benders_decomposition = false;
    parameters.verbosity_level = 0;
    rectangle::Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.full());
    EXPECT_EQ(solution.number_of_bins(), test_params.expected_number_of_bins);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleColumnGenerationVariableSizedBinPackingTest,
        testing::ValuesIn(std::vector<RectangleColumnGenerationVariableSizedBinPackingTestParams>{
            {
                // Two bin types, one copy of each: a cheap 15x15 bin too
                // small to fit both 10x10 items together, and a costlier
                // 20x30 bin that fits both at once. Using the cheap bin
                // still requires the costly one for the second item (total
                // cost 1 + 3 = 4), so the optimum (see its own
                // 'solution.csv') instead uses only the costly bin alone
                // (cost 3), for 1 bin total.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "parameters.csv",
                1,
            }}));
