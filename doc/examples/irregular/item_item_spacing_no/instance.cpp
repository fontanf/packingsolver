#include "packingsolver/irregular/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 160, 0, 100));
instance_builder.set_bin_type_copies(bin_type_id, 3);
ItemTypeId item_type_id = instance_builder.add_item_type({ItemShape{shape::ShapeWithHoles{shape::build_shape({{0, 0}, {40, 0}, {40, 20}, {20, 20}, {20, 60}, {0, 60}})}}});
instance_builder.set_item_type_copies(item_type_id, 10);
instance_builder.add_item_type_allowed_rotation(item_type_id, 0, 0, false);
instance_builder.add_item_type_allowed_rotation(item_type_id, 90, 90, false);
instance_builder.add_item_type_allowed_rotation(item_type_id, 180, 180, false);
instance_builder.add_item_type_allowed_rotation(item_type_id, 270, 270, false);
Instance instance = instance_builder.build();
