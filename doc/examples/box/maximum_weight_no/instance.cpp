#include "packingsolver/box/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(20, 20, 10);
instance_builder.set_bin_type_copies(bin_type_id, 10);
ItemTypeId item_type_id = instance_builder.add_item_type(10, 10, 10);
instance_builder.set_item_type_weight(item_type_id, 100);
instance_builder.set_item_type_copies(item_type_id, 4);
Instance instance = instance_builder.build();
