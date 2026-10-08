#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::OpenDimensionX);
instance_builder.add_bin_type(2000, 500);
ItemTypeId item_type_id = instance_builder.add_item_type(250, 150);
instance_builder.set_item_type_copies(item_type_id, 10);
item_type_id = instance_builder.add_item_type(200, 100);
instance_builder.set_item_type_copies(item_type_id, 12);
item_type_id = instance_builder.add_item_type(175, 75);
instance_builder.set_item_type_copies(item_type_id, 12);
Instance instance = instance_builder.build();
