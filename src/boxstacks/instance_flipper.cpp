#include "boxstacks/instance_flipper.hpp"

#include "packingsolver/boxstacks/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

Instance InstanceFlipper::flip(const Instance& instance)
{
    InstanceBuilder flipped_instance_builder;
    if (instance.objective() == Objective::OpenDimensionY) {
        flipped_instance_builder.set_objective(Objective::OpenDimensionX);
    } else {
        flipped_instance_builder.set_objective(instance.objective());
    }
    boxstacks::Parameters flipped_instance_parameters = instance.parameters();
    if (instance.parameters().unloading_constraint == rectangle::UnloadingConstraint::IncreasingX) {
        flipped_instance_parameters.unloading_constraint = rectangle::UnloadingConstraint::IncreasingY;
    } else if (instance.parameters().unloading_constraint == rectangle::UnloadingConstraint::IncreasingY) {
        flipped_instance_parameters.unloading_constraint = rectangle::UnloadingConstraint::IncreasingX;
    } else if (instance.parameters().unloading_constraint == rectangle::UnloadingConstraint::OnlyXMovements) {
        flipped_instance_parameters.unloading_constraint = rectangle::UnloadingConstraint::OnlyYMovements;
    } else if (instance.parameters().unloading_constraint == rectangle::UnloadingConstraint::OnlyYMovements) {
        flipped_instance_parameters.unloading_constraint = rectangle::UnloadingConstraint::OnlyXMovements;
    }
    flipped_instance_builder.set_parameters(flipped_instance_parameters);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinType bin_type_new = bin_type;
        bin_type_new.box.x = bin_type.box.y;
        bin_type_new.box.y = bin_type.box.x;
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
        item_type_new.box.x = item_type.box.y;
        item_type_new.box.y = item_type.box.x;
        flipped_instance_builder.add_item_type(
                item_type_new,
                item_type.profit,
                item_type.copies);
    }
    return flipped_instance_builder.build();
}

Solution InstanceFlipper::unflip_solution(const Solution& flipped_solution) const
{
    Solution solution(instance_orig_);
    for (BinPos bin_pos = 0;
            bin_pos < flipped_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& flipped_bin = flipped_solution.bin(bin_pos);
        solution.add_bin(
                flipped_bin.bin_type_id,
                flipped_bin.copies);
        for (StackId stack_pos = 0;
                stack_pos < (StackId)flipped_bin.stacks.size();
                ++stack_pos) {
            const SolutionStack& flipped_stack = flipped_bin.stacks[stack_pos];
            solution.add_stack(
                    bin_pos,
                    flipped_stack.y_start,
                    flipped_stack.y_end,
                    flipped_stack.x_start,
                    flipped_stack.x_end);
            for (const SolutionItem& flipped_item: flipped_stack.items) {
                solution.add_item(
                        bin_pos,
                        stack_pos,
                        flipped_item.item_type_id,
                        flipped_item.rotation);
            }
        }
    }
    return solution;
}
