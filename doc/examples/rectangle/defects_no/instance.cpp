#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
instance_builder.add_bin_type(800, 500);
instance_builder.add_bin_type(800, 500);
ItemTypeId item_type_id = instance_builder.add_item_type(400, 200, true);
instance_builder.set_item_type_copies(item_type_id, 3);
Instance instance = instance_builder.build();
