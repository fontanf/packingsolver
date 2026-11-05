#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * Compute the consumption of each resource of a bin type by some copies of
 * item types alone in the bin: the consumption of the copies of an item type
 * starts at the first entry of its schedule (see 'Resource::item_consumptions').
 *
 * Indexed by resource id. Empty iff the bin type has no resources.
 */
inline std::vector<double> compute_resource_consumption(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    std::vector<double> resource_consumption(bin_type.number_of_resources(), 0.0);
    for (const std::pair<ItemTypeId, ItemPos>& item_copy: item_copies) {
        const ItemType& item_type = instance.item_type(item_copy.first);
        for (const ItemResourceConsumption& consumption: item_type.resources[bin_type_id]) {
            const std::vector<double>& schedule = bin_type.resource(consumption.resource_id)
                .item_consumptions[consumption.consumption_pos].second;
            if (schedule.empty())
                continue;
            // A copy past the end of the schedule consumes its last entry.
            ItemPos number_of_scheduled_copies = (std::min)(
                    item_copy.second,
                    (ItemPos)schedule.size());
            for (ItemPos copy = 0; copy < number_of_scheduled_copies; ++copy)
                resource_consumption[consumption.resource_id] += schedule[copy];
            resource_consumption[consumption.resource_id]
                += (item_copy.second - number_of_scheduled_copies) * schedule.back();
        }
    }
    return resource_consumption;
}

/**
 * Return true iff adding 'added_consumption' to 'consumption' doesn't exceed
 * the capacity of a resource of the bin type which is not penalized.
 *
 * An empty 'consumption' counts as no consumption.
 */
inline bool resource_feasible(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<double>& consumption,
        const std::vector<double>& added_consumption)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (resource.penalize)
            continue;
        double previous_consumption = consumption.empty()?
            0.0: consumption[resource_id];
        if (previous_consumption + added_consumption[resource_id] > resource.capacity)
            return false;
    }
    return true;
}

/**
 * Penalty of adding 'added_consumption' to 'consumption': the sum of the
 * penalties of the penalized resources of the bin type whose capacity is
 * exceeded for the first time, as in 'Solution'.
 */
inline Profit resource_penalty(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<double>& consumption,
        const std::vector<double>& added_consumption)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Profit penalty = 0;
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (!resource.penalize)
            continue;
        double previous_consumption = consumption[resource_id];
        if (previous_consumption <= resource.capacity
                && previous_consumption + added_consumption[resource_id] > resource.capacity) {
            penalty += resource.penalty;
        }
    }
    return penalty;
}

/** Add 'added_consumption' to 'consumption'. */
inline void add_resource_consumption(
        std::vector<double>& consumption,
        const std::vector<double>& added_consumption)
{
    for (ResourceId resource_id = 0;
            resource_id < (ResourceId)consumption.size();
            ++resource_id) {
        consumption[resource_id] += added_consumption[resource_id];
    }
}

}
}
