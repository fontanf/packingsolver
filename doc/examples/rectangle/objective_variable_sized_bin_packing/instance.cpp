#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::VariableSizedBinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(1000, 500);
instance_builder.set_bin_type_copies(bin_type_id, 10);
bin_type_id = instance_builder.add_bin_type(700, 400);
instance_builder.set_bin_type_copies(bin_type_id, 10);
ItemTypeId item_type_id = instance_builder.add_item_type(250, 150);
instance_builder.set_item_type_copies(item_type_id, 10);
item_type_id = instance_builder.add_item_type(200, 100);
instance_builder.set_item_type_copies(item_type_id, 12);
item_type_id = instance_builder.add_item_type(175, 75);
instance_builder.set_item_type_copies(item_type_id, 12);
Instance instance = instance_builder.build();
