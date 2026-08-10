#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"

#include <gtest/gtest.h>

// Resources cannot be represented via the CSV instance format (rectangleguillotine
// has no JSON reader either, unlike onedimensional), so these instances are
// built directly through the InstanceBuilder API instead of the usual
// CSV-fixture-based parametrized tests.

using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineResourceTest, HardResourceForcesExtraBins)
{
    // A capacity-1 resource with each item consuming 1 caps every bin to a
    // single item, even though the bin is geometrically large enough to fit
    // several - forcing 3 bins for 3 items.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(20, 20);
    instance_builder.set_bin_type_copies(bin_type_id, 10);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0, false, 0.0);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5, 5);
    instance_builder.set_item_type_copies(item_type_id, 3);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 1.0);
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.use_tree_search = true;
    parameters.use_tree_search_maximal_spaces = false;
    parameters.use_tree_search_hypergraph_infinite_copies = false;
    parameters.use_tree_search_hypergraph = false;
    parameters.use_sequential_strips_onedimensional = false;
    parameters.use_column_generation_strips = false;
    parameters.verbosity_level = 0;
    Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.resource_feasible());
    EXPECT_EQ(solution.number_of_bins(), 3);
}

TEST(RectangleGuillotineResourceTest, PenalizeResourceDiscouragesCrossing)
{
    // Knapsack objective, item profit 10, resource capacity 1 with penalty
    // 100. Packing a 2nd item in the single available bin crosses the
    // resource once (-100), so the solver should prefer packing just 1 item
    // (profit 10) over 2 (profit 20 - 100 = -80).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(20, 20);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0, true, 100.0);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5, 5);
    instance_builder.set_item_type_profit(item_type_id, 10);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 1.0);
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.use_tree_search = true;
    parameters.use_tree_search_maximal_spaces = false;
    parameters.use_tree_search_hypergraph_infinite_copies = false;
    parameters.use_tree_search_hypergraph = false;
    parameters.use_sequential_strips_onedimensional = false;
    parameters.use_column_generation_strips = false;
    parameters.verbosity_level = 0;
    Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_EQ(solution.number_of_items(), 1);
    EXPECT_DOUBLE_EQ(solution.profit(), 10.0);
    EXPECT_TRUE(solution.resource_feasible());
}
