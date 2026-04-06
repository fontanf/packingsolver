#include "rectangle/instance_flipper.hpp"

#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

Instance InstanceFlipper::flip(const Instance& instance)
{
    InstanceBuilder flipped_instance_builder;
    if (instance.objective() == Objective::OpenDimensionY) {
        flipped_instance_builder.set_objective(Objective::OpenDimensionX);
    } else {
        flipped_instance_builder.set_objective(instance.objective());
    }
    rectangle::Parameters flipped_instance_parameters = instance.parameters();
    if (instance.parameters().unloading_constraint == UnloadingConstraint::IncreasingX) {
        flipped_instance_parameters.unloading_constraint = UnloadingConstraint::IncreasingY;
    } else if (instance.parameters().unloading_constraint == UnloadingConstraint::IncreasingY) {
        flipped_instance_parameters.unloading_constraint = UnloadingConstraint::IncreasingX;
    } else if (instance.parameters().unloading_constraint == UnloadingConstraint::OnlyXMovements) {
        flipped_instance_parameters.unloading_constraint = UnloadingConstraint::OnlyYMovements;
    } else if (instance.parameters().unloading_constraint == UnloadingConstraint::OnlyYMovements) {
        flipped_instance_parameters.unloading_constraint = UnloadingConstraint::OnlyXMovements;
    }
    flipped_instance_builder.set_parameters(flipped_instance_parameters);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeId flipped_bin_type_id = flipped_instance_builder.add_bin_type(
                bin_type.rect.y,
                bin_type.rect.x,
                bin_type.cost,
                bin_type.copies,
                bin_type.copies_min);
        flipped_instance_builder.set_bin_type_maximum_weight(
                flipped_bin_type_id,
                bin_type.maximum_weight);
        flipped_instance_builder.set_bin_type_semi_trailer_truck_parameters(
                flipped_bin_type_id,
                bin_type.semi_trailer_truck_data);
        for (const Defect& defect: bin_type.defects) {
            flipped_instance_builder.add_defect(
                    flipped_bin_type_id,
                    defect.pos.y,
                    defect.pos.x,
                    defect.rect.y,
                    defect.rect.x);
        }
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId flipped_item_type_id = flipped_instance_builder.add_item_type(
                item_type.rect.y,
                item_type.rect.x,
                item_type.profit,
                item_type.copies,
                item_type.oriented);
        flipped_instance_builder.set_item_type_group(
                flipped_item_type_id,
                item_type.group_id);
        flipped_instance_builder.set_item_type_weight(
                flipped_item_type_id,
                item_type.weight);
    }
    return flipped_instance_builder.build();
}

Solution InstanceFlipper::unflip_solution(const Solution& flipped_solution) const
{
    SolutionBuilder solution_builder(instance_orig_);
    for (BinPos bin_pos = 0;
            bin_pos < flipped_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& flipped_bin = flipped_solution.bin(bin_pos);
        solution_builder.add_bin(
                flipped_bin.bin_type_id,
                flipped_bin.copies);
        for (const SolutionItem& flipped_item: flipped_bin.items) {
            solution_builder.add_item(
                    bin_pos,
                    flipped_item.item_type_id,
                    {flipped_item.bl_corner.y, flipped_item.bl_corner.x},
                    flipped_item.rotate);
        }
    }
    return solution_builder.build();
}
