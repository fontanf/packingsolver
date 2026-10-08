#include "packingsolver/box/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::Knapsack);
instance_builder.add_bin_type(216, 173, 110);
ItemTypeId item_type_id = instance_builder.add_item_type(108, 76, 30);
instance_builder.set_item_type_copies(item_type_id, 20);
item_type_id = instance_builder.add_item_type(110, 43, 25);
instance_builder.set_item_type_copies(item_type_id, 20);
item_type_id = instance_builder.add_item_type(92, 81, 55);
instance_builder.set_item_type_copies(item_type_id, 20);
Instance instance = instance_builder.build();
