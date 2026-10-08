#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
instance_builder.set_number_of_stages(2);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
instance_builder.set_bin_type_copies(bin_type_id, 3);
ItemTypeId item_type_id = instance_builder.add_item_type(3, 10, true);
instance_builder.set_item_type_copies(item_type_id, 3);
Instance instance = instance_builder.build();
