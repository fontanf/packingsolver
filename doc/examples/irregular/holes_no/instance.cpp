#include "packingsolver/irregular/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 40, 0, 40));
instance_builder.set_bin_type_copies(bin_type_id, 2);
ItemTypeId item_type_id = instance_builder.add_item_type({ItemShape{shape::ShapeWithHoles{shape::build_shape({{0, 0}, {40, 0}, {40, 30}, {30, 40}, {0, 40}})}}});
instance_builder.set_item_type_copies(item_type_id, 1);
item_type_id = instance_builder.add_item_type({ItemShape{shape::ShapeWithHoles{shape::build_shape({{0, 0}, {12, 0}, {6, 12}})}}});
instance_builder.set_item_type_copies(item_type_id, 1);
Instance instance = instance_builder.build();
