#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(12, 9);
instance_builder.set_bin_type_copies(bin_type_id, 2);
instance_builder.add_item_type(8, 3, true);
instance_builder.add_item_type(4, 5, true);
instance_builder.add_item_type(5, 4, true);
instance_builder.add_item_type(7, 6, true);
Instance instance = instance_builder.build();
