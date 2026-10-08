#include "packingsolver/onedimensional/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(500);
instance_builder.set_bin_type_copies(bin_type_id, 10);
ItemTypeId item_type_id = instance_builder.add_item_type(200);
instance_builder.set_item_type_maximum_stackability(item_type_id, 3);
instance_builder.set_item_type_copies(item_type_id, 3);
item_type_id = instance_builder.add_item_type(100);
instance_builder.set_item_type_maximum_stackability(item_type_id, 100);
instance_builder.set_item_type_copies(item_type_id, 4);
Instance instance = instance_builder.build();
