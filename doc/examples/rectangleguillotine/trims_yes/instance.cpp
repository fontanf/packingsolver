#include "packingsolver/rectangleguillotine/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

InstanceBuilder instance_builder;
instance_builder.set_objective(Objective::BinPacking);
BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
instance_builder.set_bin_type_copies(bin_type_id, 3);
instance_builder.add_trims(
        bin_type_id,
        1, TrimType::Hard,
        1, TrimType::Soft,
        1, TrimType::Hard,
        1, TrimType::Soft);
instance_builder.add_item_type(4, 8, true);
instance_builder.add_item_type(3, 8, true);
instance_builder.add_item_type(3, 8, true);
Instance instance = instance_builder.build();
