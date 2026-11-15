#include "packingsolver/box/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
instance_builder.set_static_stability(true);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 20);
instance_builder.set_bin_type_copies(bin_type_id, 10);
ItemTypeId item_type_id = instance_builder.add_item_type(9, 3, 12);
instance_builder.set_item_type_copies(item_type_id, 2);
item_type_id = instance_builder.add_item_type(10, 7, 6);
instance_builder.set_item_type_copies(item_type_id, 2);
Instance instance = instance_builder.build();
