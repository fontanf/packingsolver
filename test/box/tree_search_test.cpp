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

TEST(BoxBranchingScheme, DominanceCopyDependentResourceConsumption)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    packingsolver::ItemTypeId item_type_id_a = instance_builder.add_item_type(1, 1, 1);
    instance_builder.set_item_type_copies(item_type_id_a, 5);
    packingsolver::ItemTypeId item_type_id_b = instance_builder.add_item_type(1, 1, 1);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_a, {1.0, 1.0, 0.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_b, {1.0});
    const Instance instance = instance_builder.build();
    BranchingScheme branching_scheme(instance, BranchingScheme::Parameters());
    // Child of 'parent' with one more copy of 'item_type_id', in a new bin
    // or in the current one; 'nullptr' if there is none.
    auto insert = [&branching_scheme](
            const std::shared_ptr<BranchingScheme::Node>& parent,
            packingsolver::ItemTypeId item_type_id,
            bool new_bin) -> std::shared_ptr<BranchingScheme::Node>
    {
        if (parent == nullptr)
            return nullptr;
        for (const auto& child: branching_scheme.children(parent)) {
            if (child->number_of_bins == parent->number_of_bins + (new_bin? 1: 0)
                    && child->item_number_of_copies[item_type_id]
                    == parent->item_number_of_copies[item_type_id] + 1) {
                return child;
            }
        }
        return nullptr;
    };
    // A resource of capacity 2: the first two copies of item type A in a bin
    // consume 1 each, the next ones nothing; item type B consumes 1. Two
    // nodes with the same geometry, whose last bins both consume 2: one with
    // a copy of A and a copy of B, the other with two copies of A. Another
    // copy of A fits in the second one (its third copy consumes nothing),
    // not in the first one (its second copy consumes 1), so the first one
    // doesn't dominate the second one, although it doesn't consume more.
    auto root = branching_scheme.root();
    auto node = insert(insert(root, item_type_id_a, true), item_type_id_a, false);
    ASSERT_NE(node, nullptr);
    auto node_1 = std::make_shared<BranchingScheme::Node>(*node);
    node_1->last_bin_resource_consumption = {2};
    node_1->last_bin_item_number_of_copies = {1, 1};
    auto node_2 = std::make_shared<BranchingScheme::Node>(*node);
    node_2->last_bin_resource_consumption = {2};
    node_2->last_bin_item_number_of_copies = {2, 0};
    EXPECT_FALSE(branching_scheme.dominates(node_1, node_2));
    EXPECT_FALSE(branching_scheme.dominates(node_2, node_1));
    // With the same copies, the consumptions decide.
    node_1->last_bin_item_number_of_copies = {2, 0};
    node_1->last_bin_resource_consumption = {1};
    EXPECT_TRUE(branching_scheme.dominates(node_1, node_2));
}

TEST(BoxBranchingScheme, PredecessorsDifferentResourceConsumptions)
{
    // Item types A and B have the same dimensions and the same profit, but
    // only A consumes the resource: packing B first may matter, so B must not
    // wait for the copies of A.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1);
    packingsolver::ItemTypeId item_type_id_a = instance_builder.add_item_type(1, 1, 1);
    instance_builder.set_item_type_copies(item_type_id_a, 2);
    packingsolver::ItemTypeId item_type_id_b = instance_builder.add_item_type(1, 1, 1);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_a, {1.0});
    const Instance instance = instance_builder.build();
    BranchingScheme branching_scheme(instance, BranchingScheme::Parameters());

    bool b_first = false;
    for (const auto& child: branching_scheme.children(branching_scheme.root()))
        if (child->item_number_of_copies[item_type_id_b] == 1)
            b_first = true;
    EXPECT_TRUE(b_first);
}
