#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::OpenDimensionX);
instance_builder.set_unloading_constraint(UnloadingConstraint::OnlyXMovements);
instance_builder.add_bin_type(800, 400);
instance_builder.add_item_type(500, 200);
ItemTypeId item_type_id = instance_builder.add_item_type(200, 200);
instance_builder.set_item_type_group(item_type_id, 1);
item_type_id = instance_builder.add_item_type(150, 150);
instance_builder.set_item_type_group(item_type_id, 1);
Instance instance = instance_builder.build();
