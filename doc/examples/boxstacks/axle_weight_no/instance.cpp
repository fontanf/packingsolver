#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(13500, 2440, 2900);
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type(1200, 1000, 1200);
instance_builder.set_item_type_weight(item_type_id, 1200);
instance_builder.set_item_type_copies(item_type_id, 19);
item_type_id = instance_builder.add_item_type(1200, 1000, 800);
instance_builder.set_item_type_weight(item_type_id, 800);
instance_builder.set_item_type_copies(item_type_id, 19);
Instance instance = instance_builder.build();
