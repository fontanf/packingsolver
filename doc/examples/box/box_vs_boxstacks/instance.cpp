#include "packingsolver/box/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::Knapsack);
instance_builder.add_bin_type(7500, 2400, 3000);
ItemTypeId item_type_id = instance_builder.add_item_type(1300, 1200, 1500);
instance_builder.set_item_type_copies(item_type_id, 4);
item_type_id = instance_builder.add_item_type(1300, 1200, 550);
instance_builder.set_item_type_copies(item_type_id, 8);
item_type_id = instance_builder.add_item_type(1200, 1200, 1300);
instance_builder.set_item_type_copies(item_type_id, 4);
item_type_id = instance_builder.add_item_type(1200, 1200, 450);
instance_builder.set_item_type_copies(item_type_id, 12);
item_type_id = instance_builder.add_item_type(1250, 1200, 1600);
instance_builder.set_item_type_copies(item_type_id, 4);
item_type_id = instance_builder.add_item_type(1250, 1200, 1100);
instance_builder.set_item_type_copies(item_type_id, 4);
item_type_id = instance_builder.add_item_type(600, 500, 400);
instance_builder.set_item_type_copies(item_type_id, 4);
item_type_id = instance_builder.add_item_type(500, 400, 350);
instance_builder.set_item_type_copies(item_type_id, 4);
item_type_id = instance_builder.add_item_type(700, 600, 300);
instance_builder.set_item_type_copies(item_type_id, 4);
Instance instance = instance_builder.build();
