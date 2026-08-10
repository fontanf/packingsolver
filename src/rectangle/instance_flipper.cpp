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
                bin_type.rect.x);
        flipped_instance_builder.set_bin_type_cost(
                flipped_bin_type_id,
                bin_type.cost);
        flipped_instance_builder.set_bin_type_copies(
                flipped_bin_type_id,
                bin_type.copies);
        flipped_instance_builder.set_bin_type_copies_min(
                flipped_bin_type_id,
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
        // Copy resources (their consumptions are copied below, once item
        // types have been added).
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            flipped_instance_builder.add_bin_type_resource(
                    flipped_bin_type_id,
                    resource.capacity,
                    resource.penalize,
                    resource.penalty);
        }
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId flipped_item_type_id = flipped_instance_builder.add_item_type(
                item_type.rect.y,
                item_type.rect.x,
                item_type.oriented);
        flipped_instance_builder.set_item_type_profit(
                flipped_item_type_id,
                item_type.profit);
        flipped_instance_builder.set_item_type_copies(
                flipped_item_type_id,
                item_type.copies);
        flipped_instance_builder.set_item_type_group(
                flipped_item_type_id,
                item_type.group_id);
        flipped_instance_builder.set_item_type_weight(
                flipped_item_type_id,
                item_type.weight);
    }
    // Copy resource consumptions, now that both bin and item types have
    // been added (bin type ids and item type ids line up 1:1 with
    // 'instance's own, so no remapping is needed).
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            for (ItemTypeId item_type_id = 0;
                    item_type_id < (ItemTypeId)resource.item_consumptions.size();
                    ++item_type_id) {
                const std::vector<double>& schedule = resource.item_consumptions[item_type_id];
                for (ItemPos item_copy = 0;
                        item_copy < (ItemPos)schedule.size();
                        ++item_copy) {
                    flipped_instance_builder.add_resource_consumption(
                            bin_type_id,
                            resource_id,
                            item_type_id,
                            item_copy,
                            schedule[item_copy]);
                }
            }
        }
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
