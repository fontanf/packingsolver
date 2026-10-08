#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 12);
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type(6, 10, true);
instance_builder.set_item_type_copies(item_type_id, 2);
Instance instance = instance_builder.build();
