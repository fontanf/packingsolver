#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

struct BoxStacksOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
    /** Time limit of the run; the default leaves the optimization unlimited. */
    double time_limit = std::numeric_limits<double>::infinity();
    /** Anytime variant of the sequential one-dimensional rectangle algorithm. */
    bool sequential_onedimensional_rectangle_anytime = false;
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
    optimize_parameters.sequential_onedimensional_rectangle_anytime = test_params.sequential_onedimensional_rectangle_anytime;
    optimize_parameters.timer.set_time_limit(test_params.time_limit);
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
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "solution.csv",
            }, {
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "solution.csv",
            }, {
                // Same instance with the anytime variant of the sequential
                // one-dimensional rectangle algorithm: its rectangle search grows
                // its queue from 1 and reports every improvement (see
                // 'rectangle_solution_to_boxstacks'); the optimum (two pallets,
                // the volume bound) is reached either way.
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_postal_cartons_eur_pallets" / "solution.csv",
                std::numeric_limits<double>::infinity(),
                true,
            },
            // The three instances below need a time limit to be meaningful:
            // with more items than fit and more than one item type, solving
            // the full 'box' relaxation to (near-)optimality used to consume
            // the whole time limit before the primal algorithms ever ran
            // (see 'optimize_box_bound' in 'optimize.cpp'), so no solution
            // was returned at all.
            {
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "knapsack_two_item_types_pallet_time_limit" / "solution.csv",
                3.0,
            }, {
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "bin_packing_two_item_types_pallets_time_limit" / "solution.csv",
                3.0,
            }, {
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "parameters.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_pallet_types_time_limit" / "solution.csv",
                3.0,
            }}));
