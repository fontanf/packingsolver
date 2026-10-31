#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"
#include "box/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::box;
namespace fs = boost::filesystem;

struct BoxTreeSearchTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const BoxTreeSearchTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxTreeSearchTest: public testing::TestWithParam<BoxTreeSearchTestParams> { };

TEST_P(BoxTreeSearchTest, BoxTreeSearch)
{
    BoxTreeSearchTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    //optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
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
        BoxTreeSearch,
        BoxTreeSearchTest,
        testing::ValuesIn(std::vector<BoxTreeSearchTestParams>{
            {
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "items.csv",
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "bins.csv",
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "items.csv",
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "bins.csv",
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "items.csv",
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "bins.csv",
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_y_4_different_items_xy" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_y_4_different_items_xy" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_y_4_different_items_xy" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_y_4_different_items_xy" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_z_4_different_items_zy" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_z_4_different_items_zy" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_z_4_different_items_zy" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_z_4_different_items_zy" / "solution.csv",
            }}));

#include "box/tree_search.hpp"

TEST(BoxBranchingScheme, DominanceComparesTheResourceConsumptionOfTheCurrentBin)
{
    // Two nodes with the same items, the same number of bins and the same
    // skyline, but node 1 consumes more of the resource of its current bin:
    // an item which still fits in the current bin of node 2 may not fit in
    // the current bin of node 1, so node 1 doesn't dominate node 2.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, 5);
    instance_builder.set_item_type_copies(item_type_id, 4);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, {1});
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters parameters;
    BranchingScheme branching_scheme(instance, parameters);

    BranchingScheme::UncoveredItem uncovered_item;
    uncovered_item.x = 0;
    uncovered_item.x_dominance = 0;
    uncovered_item.ys = 0;
    uncovered_item.ye = 10;
    uncovered_item.zs = 0;
    uncovered_item.ze = 10;
    auto node_1 = std::make_shared<BranchingScheme::Node>();
    node_1->number_of_bins = 1;
    node_1->uncovered_items = {uncovered_item};
    node_1->last_bin_resource_consumption = {2};
    auto node_2 = std::make_shared<BranchingScheme::Node>(*node_1);
    node_2->last_bin_resource_consumption = {1};

    EXPECT_FALSE(branching_scheme.dominates(node_1, node_2));
    EXPECT_TRUE(branching_scheme.dominates(node_2, node_1));
}
