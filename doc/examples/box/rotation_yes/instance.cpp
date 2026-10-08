#include "packingsolver/box/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type(10, 10, 6);
instance_builder.add_item_type_rotation(item_type_id, Rotation::XYZ);
item_type_id = instance_builder.add_item_type(10, 4, 6);
instance_builder.add_item_type_rotation(item_type_id, Rotation::XYZ);
instance_builder.add_item_type_rotation(item_type_id, Rotation::XZY);
Instance instance = instance_builder.build();
