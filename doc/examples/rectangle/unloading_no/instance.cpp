#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
BinTypeId bin_type_id = instance_builder.add_bin_type(700, 400);
instance_builder.set_bin_type_copies(bin_type_id, 2);
instance_builder.add_item_type(200, 200);
ItemTypeId item_type_id = instance_builder.add_item_type(400, 400);
instance_builder.set_item_type_group(item_type_id, 1);
item_type_id = instance_builder.add_item_type(150, 150);
instance_builder.set_item_type_group(item_type_id, 2);
Instance instance = instance_builder.build();
