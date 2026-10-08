#include "packingsolver/rectangle/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPackingWithLeftovers);
BinTypeId bin_type_id = instance_builder.add_bin_type(1000, 500);
instance_builder.set_bin_type_copies(bin_type_id, 20);
instance_builder.add_item_type(100, 50);
instance_builder.add_item_type(106, 56);
instance_builder.add_item_type(112, 62);
instance_builder.add_item_type(118, 68);
instance_builder.add_item_type(124, 74);
instance_builder.add_item_type(130, 80);
instance_builder.add_item_type(136, 86);
instance_builder.add_item_type(142, 92);
instance_builder.add_item_type(148, 98);
instance_builder.add_item_type(154, 104);
instance_builder.add_item_type(160, 110);
instance_builder.add_item_type(166, 116);
instance_builder.add_item_type(172, 122);
instance_builder.add_item_type(178, 128);
instance_builder.add_item_type(184, 134);
instance_builder.add_item_type(190, 140);
instance_builder.add_item_type(196, 146);
instance_builder.add_item_type(202, 152);
instance_builder.add_item_type(208, 158);
instance_builder.add_item_type(214, 164);
Instance instance = instance_builder.build();
