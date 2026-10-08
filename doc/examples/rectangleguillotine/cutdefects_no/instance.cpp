#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
instance_builder.set_number_of_stages(2);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
instance_builder.add_defect(bin_type_id, 4, 7, 2, 2);
bin_type_id = instance_builder.add_bin_type(10, 10);
instance_builder.set_bin_type_copies(bin_type_id, 2);
instance_builder.add_item_type(5, 5, true);
instance_builder.add_item_type(5, 6, true);
Instance instance = instance_builder.build();
