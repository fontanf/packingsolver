#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
instance_builder.set_first_stage_orientation(CutOrientation::Vertical);
instance_builder.set_maximum_number_2_cuts(1);
BinTypeId bin_type_id = instance_builder.add_bin_type(20, 10);
instance_builder.set_bin_type_copies(bin_type_id, 3);
instance_builder.add_item_type(10, 3, true);
instance_builder.add_item_type(10, 3, true);
instance_builder.add_item_type(10, 4, true);
instance_builder.add_item_type(10, 10, true);
Instance instance = instance_builder.build();
