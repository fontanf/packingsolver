#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
instance_builder.set_number_of_stages(2);
instance_builder.set_first_stage_orientation(CutOrientation::Horizontal);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
instance_builder.set_bin_type_copies(bin_type_id, 3);
instance_builder.add_item_type(4, 3, true);
instance_builder.add_item_type(4, 7, true);
instance_builder.add_item_type(6, 5, true);
instance_builder.add_item_type(6, 4, true);
Instance instance = instance_builder.build();
