#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(7500, 2400, 3000);
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type(3000, 2400, 1500);
instance_builder.set_item_type_weight(item_type_id, 40);
instance_builder.set_item_type_copies(item_type_id, 2);
item_type_id = instance_builder.add_item_type(3000, 2400, 1000);
instance_builder.set_item_type_weight(item_type_id, 20);
instance_builder.set_item_type_copies(item_type_id, 3);
Instance instance = instance_builder.build();
