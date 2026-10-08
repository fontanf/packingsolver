#include "packingsolver/irregular/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 160, 0, 120));
instance_builder.set_bin_type_copies(bin_type_id, 1);
DefectId defect_id = instance_builder.add_defect(bin_type_id, -1, shape::ShapeWithHoles{shape::build_shape({{40, 40}, {60, 40}, {40, 60}})});
instance_builder.set_item_defect_minimum_spacing(bin_type_id, defect_id, 0);
bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 160, 0, 120));
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type({ItemShape{shape::ShapeWithHoles{shape::build_shape({{0, 0}, {40, 0}, {0, 40}})}}});
instance_builder.set_item_type_copies(item_type_id, 23);
instance_builder.add_item_type_allowed_rotation(item_type_id, 0, 0, false);
instance_builder.add_item_type_allowed_rotation(item_type_id, 90, 90, false);
instance_builder.add_item_type_allowed_rotation(item_type_id, 180, 180, false);
instance_builder.add_item_type_allowed_rotation(item_type_id, 270, 270, false);
Instance instance = instance_builder.build();
