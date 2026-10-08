#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
BinTypeId bin_type_id = instance_builder.add_bin_type(1000, 700);
instance_builder.set_bin_type_copies(bin_type_id, 5);
ItemTypeId item_type_id = instance_builder.add_item_type(250, 200);
instance_builder.set_item_type_copies(item_type_id, 2);
item_type_id = instance_builder.add_item_type(150, 300);
instance_builder.set_item_type_copies(item_type_id, 2);
item_type_id = instance_builder.add_item_type(200, 150);
instance_builder.set_item_type_copies(item_type_id, 3);
Instance instance = instance_builder.build();
