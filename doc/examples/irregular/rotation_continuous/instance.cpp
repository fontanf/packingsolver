#include "packingsolver/irregular/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::OpenDimensionXY);
instance_builder.set_open_dimension_xy_aspect_ratio(1);
instance_builder.add_bin_type(shape::build_rectangle(0, 300, 0, 300));
ItemTypeId item_type_id = instance_builder.add_item_type({ItemShape{shape::ShapeWithHoles{shape::build_shape({{-12.5, 50}, {-6.25, 50}, {-20, 25}, {-10, 25}, {-35, 0}, {-7.5, 0}, {-7.5, -20}, {7.5, -20}, {7.5, 0}, {35, 0}, {10, 25}, {20, 25}, {6.25, 50}, {12.5, 50}, {0, 80}})}}});
instance_builder.set_item_type_copies(item_type_id, 7);
instance_builder.add_item_type_allowed_rotation(item_type_id, 0, 360, false);
Instance instance = instance_builder.build();
