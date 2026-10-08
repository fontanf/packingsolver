#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
instance_builder.set_unloading_constraint(rectangle::UnloadingConstraint::OnlyXMovements);
BinTypeId bin_type_id = instance_builder.add_bin_type(7500, 2400, 3000);
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type(3000, 1400, 1000);
instance_builder.set_item_type_stackability_id(item_type_id, 0);
item_type_id = instance_builder.add_item_type(3000, 2400, 1000);
instance_builder.set_item_type_group(item_type_id, 1);
instance_builder.set_item_type_stackability_id(item_type_id, 1);
item_type_id = instance_builder.add_item_type(2500, 1000, 1000);
instance_builder.set_item_type_group(item_type_id, 2);
instance_builder.set_item_type_stackability_id(item_type_id, 2);
Instance instance = instance_builder.build();
