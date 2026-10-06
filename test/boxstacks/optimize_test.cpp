#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

struct BoxStacksOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const BoxStacksOptimizeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxStacksOptimizeTest: public testing::TestWithParam<BoxStacksOptimizeTestParams> { };

TEST_P(BoxStacksOptimizeTest, BoxStacksOptimize)
{
    BoxStacksOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        BoxStacks,
        BoxStacksOptimizeTest,
        testing::ValuesIn(std::vector<BoxStacksOptimizeTestParams>{
            {
                // Large volumes: the one-dimensional relaxation used to need
                // several GB (subset sum dynamic programming over the volume
                // of the bin in its reduction).
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_large_volumes" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_large_volumes" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_large_volumes" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_large_volumes" / "solution.csv",
            }, {
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "solution.csv",
            }, {
                // Bin packing with leftovers: the two items should be placed
                // side by side along X, leaving the leftover along Y.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_with_leftovers_leftover_along_y" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_with_leftovers_leftover_along_y" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_with_leftovers_leftover_along_y" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_with_leftovers_leftover_along_y" / "solution.csv",
            }, {
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "solution.csv",
            },
            {
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "solution.csv",
            }, {
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "solution.csv",
            }, {
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "solution.csv",
            }, {
                // Open dimension Y: the bin must be filled along Y. Filling it
                // along X with the sequential onedimensional rectangle
                // algorithm leads to a worse solution.
                fs::path("data") / "boxstacks" / "tests" / "open_dimension_y_rotated_items" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "open_dimension_y_rotated_items" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "open_dimension_y_rotated_items" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "open_dimension_y_rotated_items" / "solution.csv",
            }}));
