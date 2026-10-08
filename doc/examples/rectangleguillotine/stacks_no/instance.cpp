#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
instance_builder.set_bin_type_copies(bin_type_id, 4);
instance_builder.add_item_type(5, 6, true);
instance_builder.add_item_type(5, 7, true);
instance_builder.add_item_type(5, 3, true);
instance_builder.add_item_type(5, 4, true);
Instance instance = instance_builder.build();
