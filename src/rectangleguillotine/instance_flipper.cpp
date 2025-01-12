#include "rectangleguillotine/instance_flipper.hpp"

#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

Instance InstanceFlipper::flip(const Instance& instance)
{
    // Build flipped instance.
    InstanceBuilder flipped_instance_builder;
    if (instance.objective() == Objective::OpenDimensionY) {
        flipped_instance_builder.set_objective(Objective::OpenDimensionX);
    } else {
        flipped_instance_builder.set_objective(instance.objective());
    }
    rectangleguillotine::Parameters flipped_instance_parameters = instance.parameters();
    flipped_instance_parameters.first_stage_orientation = CutOrientation::Vertical;
    flipped_instance_builder.set_parameters(flipped_instance_parameters);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinType bin_type_new = bin_type;
        bin_type_new.rect.w = bin_type.rect.h;
        bin_type_new.rect.h = bin_type.rect.w;
        bin_type_new.left_trim = bin_type.bottom_trim;
        bin_type_new.left_trim_type = bin_type.bottom_trim_type;
        bin_type_new.right_trim = bin_type.top_trim;
        bin_type_new.right_trim_type = bin_type.top_trim_type;
        bin_type_new.bottom_trim = bin_type.left_trim;
        bin_type_new.bottom_trim_type = bin_type.left_trim_type;
        bin_type_new.top_trim = bin_type.right_trim;
        bin_type_new.top_trim_type = bin_type.right_trim_type;
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            bin_type_new.defects[defect_id].pos.x = bin_type.defects[defect_id].pos.y;
            bin_type_new.defects[defect_id].pos.y = bin_type.defects[defect_id].pos.x;
            bin_type_new.defects[defect_id].rect.w = bin_type.defects[defect_id].rect.h;
            bin_type_new.defects[defect_id].rect.h = bin_type.defects[defect_id].rect.w;
        }
        flipped_instance_builder.add_bin_type(
                bin_type_new,
                bin_type.copies,
                bin_type.copies_min);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemType item_type_new = item_type;
        item_type_new.rect.w = item_type.rect.h;
        item_type_new.rect.h = item_type.rect.w;
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
                flipped_bin.copies,
                CutOrientation::Horizontal);
        for (const SolutionNode& flipped_node: flipped_bin.nodes) {
            if (flipped_node.d <= 0)
                continue;
            if (flipped_node.d % 2 == 1) {
                solution_builder.add_node(flipped_node.d, flipped_node.r);
            } else {
                solution_builder.add_node(flipped_node.d, flipped_node.t);
            }
            if (flipped_node.item_type_id >= 0)
                solution_builder.set_last_node_item(flipped_node.item_type_id);
        }
    }
    return solution_builder.build();
}
