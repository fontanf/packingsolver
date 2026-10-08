#include "packingsolver/onedimensional/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
BinTypeId bin_type_id = instance_builder.add_bin_type(500);
instance_builder.set_bin_type_copies(bin_type_id, 10);
ItemTypeId item_type_id = instance_builder.add_item_type(240);
instance_builder.set_item_type_weight(item_type_id, 200);
instance_builder.set_item_type_maximum_weight_after(item_type_id, 150);
instance_builder.set_item_type_copies(item_type_id, 2);
item_type_id = instance_builder.add_item_type(160);
instance_builder.set_item_type_weight(item_type_id, 100);
instance_builder.set_item_type_maximum_weight_after(item_type_id, 10000);
instance_builder.set_item_type_copies(item_type_id, 3);
Instance instance = instance_builder.build();
