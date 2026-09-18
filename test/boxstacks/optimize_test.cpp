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

TEST(BoxStacksOptimize, AnytimeQueueGrowthStopsAtNotAnytimeSizes)
{
    // A 40' HQ container with five cargo types and 1036 items, knapsack: the
    // best known packing has 1022 items, so no level packs everything, no
    // bound proves optimality, and the rectangle search does not exhaust its
    // tree at these sizes, so 'rectangle_subproblem_explored_exhaustively'
    // never fires either. The only thing that ends the anytime growth loop
    // of 'optimize_sequential_onedimensional_rectangle' is reaching the
    // 'not_anytime_*' queue sizes. Lowered here to 64 / 32, the levels are
    // the growth factors 1, 2, 4, 8, 16 and 32 (rectangle queues 2 to 64,
    // tree search queues 1 to 32): six SOR calls, a couple of seconds. Without
    // that bound the loop keeps doubling both queues until the safety time
    // limit below, and would never return without one.
    InstanceBuilder instance_builder;
    instance_builder.read_item_types((fs::path("data") / "boxstacks" / "tests" / "knapsack_forty_hq_five_cargo_types" / "items.csv").string());
    instance_builder.read_bin_types((fs::path("data") / "boxstacks" / "tests" / "knapsack_forty_hq_five_cargo_types" / "bins.csv").string());
    instance_builder.read_parameters((fs::path("data") / "boxstacks" / "tests" / "knapsack_forty_hq_five_cargo_types" / "parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::Anytime;
    // The algorithm counters live on the 'Output' of the solve that runs the
    // algorithms; with the reduction wrapper on, the top-level 'Output' only
    // receives its solutions and bounds. Nothing is reducible here anyway.
    optimize_parameters.reduction_parameters.reduce = false;
    optimize_parameters.not_anytime_sequential_onedimensional_rectangle_rectangle_tree_search_queue_size = 64;
    optimize_parameters.not_anytime_tree_search_queue_size = 32;
    optimize_parameters.timer.set_time_limit(60);
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.number_of_sequential_onedimensional_rectangle_calls, 6);
    EXPECT_LT(output.time, 60.0);
    EXPECT_GT(output.solution_pool.best().number_of_items(), 0);
    EXPECT_LT(output.solution_pool.best().number_of_items(), instance.number_of_items());
}
