#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::Knapsack);
instance_builder.add_bin_type(7500, 2400, 3000);
ItemTypeId item_type_id = instance_builder.add_item_type(2500, 800, 750);
instance_builder.set_item_type_stackability_id(item_type_id, 0);
instance_builder.set_item_type_copies(item_type_id, 10);
item_type_id = instance_builder.add_item_type(2500, 800, 1000);
instance_builder.set_item_type_stackability_id(item_type_id, 1);
instance_builder.set_item_type_copies(item_type_id, 10);
item_type_id = instance_builder.add_item_type(2500, 800, 1250);
instance_builder.set_item_type_stackability_id(item_type_id, 2);
instance_builder.set_item_type_copies(item_type_id, 10);
Instance instance = instance_builder.build();
