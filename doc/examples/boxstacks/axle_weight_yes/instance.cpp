#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(13500, 2440, 2900);
instance_builder.set_bin_type_copies(bin_type_id, 2);
SemiTrailerTruckData semi_trailer_truck_data;
semi_trailer_truck_data.is = true;
semi_trailer_truck_data.tractor_weight = 7808;
semi_trailer_truck_data.front_axle_middle_axle_distance = 3800;
semi_trailer_truck_data.front_axle_tractor_gravity_center_distance = 1040;
semi_trailer_truck_data.front_axle_harness_distance = 3330;
semi_trailer_truck_data.empty_trailer_weight = 7300;
semi_trailer_truck_data.harness_rear_axle_distance = 7630;
semi_trailer_truck_data.trailer_gravity_center_rear_axle_distance = 2350;
semi_trailer_truck_data.trailer_start_harness_distance = 1670;
semi_trailer_truck_data.rear_axle_maximum_weight = 31500;
semi_trailer_truck_data.middle_axle_maximum_weight = 12000;
instance_builder.set_bin_type_semi_trailer_truck_parameters(bin_type_id, semi_trailer_truck_data);
ItemTypeId item_type_id = instance_builder.add_item_type(1200, 1000, 1200);
instance_builder.set_item_type_weight(item_type_id, 1200);
instance_builder.set_item_type_copies(item_type_id, 19);
item_type_id = instance_builder.add_item_type(1200, 1000, 800);
instance_builder.set_item_type_weight(item_type_id, 800);
instance_builder.set_item_type_copies(item_type_id, 19);
Instance instance = instance_builder.build();
