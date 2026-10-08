#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
instance_builder.set_number_of_stages(3);
instance_builder.set_cut_type(CutType::Homogenous);
instance_builder.set_first_stage_orientation(CutOrientation::Vertical);
BinTypeId bin_type_id = instance_builder.add_bin_type(80, 40);
instance_builder.set_bin_type_copies(bin_type_id, 7);
ItemTypeId item_type_id = instance_builder.add_item_type(23, 23, true);
instance_builder.set_item_type_copies(item_type_id, 3);
item_type_id = instance_builder.add_item_type(19, 19, true);
instance_builder.set_item_type_copies(item_type_id, 3);
item_type_id = instance_builder.add_item_type(17, 17, true);
instance_builder.set_item_type_copies(item_type_id, 3);
item_type_id = instance_builder.add_item_type(13, 13, true);
instance_builder.set_item_type_copies(item_type_id, 3);
instance_builder.add_item_type(13, 4, true);
instance_builder.add_item_type(13, 10, true);
instance_builder.add_item_type(12, 20, true);
instance_builder.add_item_type(9, 11, true);
instance_builder.add_item_type(11, 11, true);
instance_builder.add_item_type(13, 11, true);
item_type_id = instance_builder.add_item_type(7, 7, true);
instance_builder.set_item_type_copies(item_type_id, 3);
item_type_id = instance_builder.add_item_type(5, 5, true);
instance_builder.set_item_type_copies(item_type_id, 3);
Instance instance = instance_builder.build();
