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
        BinTypeId flipped_bin_type_id = flipped_instance_builder.add_bin_type(
                bin_type.box.y,
                bin_type.box.x,
                bin_type.box.z,
                bin_type.cost,
                bin_type.copies);
        flipped_instance_builder.set_bin_type_maximum_weight(
                flipped_bin_type_id,
                bin_type.maximum_weight);
        flipped_instance_builder.set_bin_type_maximum_stack_density(
                flipped_bin_type_id,
                bin_type.maximum_stack_density);
        flipped_instance_builder.set_bin_type_semi_trailer_truck_parameters(
                flipped_bin_type_id,
                bin_type.semi_trailer_truck_data);
        for (const auto& defect: bin_type.defects) {
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
                item_type.box.y,
                item_type.box.x,
                item_type.box.z,
                item_type.profit,
                item_type.copies);
        for (Rotation rotation: item_type.rotations)
            flipped_instance_builder.add_item_type_rotation(flipped_item_type_id, rotation);
        flipped_instance_builder.set_item_type_group(
                flipped_item_type_id,
                item_type.group_id);
        flipped_instance_builder.set_item_type_weight(
                flipped_item_type_id,
                item_type.weight);
        flipped_instance_builder.set_item_type_stackability_id(
                flipped_item_type_id,
                item_type.stackability_id);
        flipped_instance_builder.set_item_type_nesting_height(
                flipped_item_type_id,
                item_type.nesting_height);
        flipped_instance_builder.set_item_type_maximum_stackability(
                flipped_item_type_id,
                item_type.maximum_stackability);
        flipped_instance_builder.set_item_type_maximum_weight_above(
                flipped_item_type_id,
                item_type.maximum_weight_above);
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
