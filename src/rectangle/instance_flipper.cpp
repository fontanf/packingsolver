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
        BinType bin_type_new = bin_type;
        bin_type_new.rect.x = bin_type.rect.y;
        bin_type_new.rect.y = bin_type.rect.x;
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            bin_type_new.defects[defect_id].pos.x = bin_type.defects[defect_id].pos.y;
            bin_type_new.defects[defect_id].pos.y = bin_type.defects[defect_id].pos.x;
            bin_type_new.defects[defect_id].rect.x = bin_type.defects[defect_id].rect.y;
            bin_type_new.defects[defect_id].rect.y = bin_type.defects[defect_id].rect.x;
        }
        flipped_instance_builder.add_bin_type(
                bin_type_new,
                bin_type.copies);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemType item_type_new = item_type;
        item_type_new.rect.x = item_type.rect.y;
        item_type_new.rect.y = item_type.rect.x;
        flipped_instance_builder.add_item_type(
                item_type_new,
                item_type.profit,
                item_type.copies);
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
