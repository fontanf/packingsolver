#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(7500, 2400, 3000);
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type(2400, 1250, 1000);
instance_builder.set_item_type_copies(item_type_id, 18);
instance_builder.add_item_type_rotation(item_type_id, Rotation::XYZ);
instance_builder.add_item_type_rotation(item_type_id, Rotation::YXZ);
Instance instance = instance_builder.build();
