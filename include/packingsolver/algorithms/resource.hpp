#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <cmath>
#include <vector>

namespace packingsolver
{

/**
 * A resource of a bin type: a capacity, together with each item type's
 * consumption of it. Shared across every problem type so that a resource
 * (and, in particular, a combinatorial cut expressed as one - see below)
 * means the same thing everywhere; see e.g. 'onedimensional::BinType::resources'
 * / 'InstanceBuilder::add_bin_type_resource' /
 * 'InstanceBuilder::add_resource_consumption' for one domain's wiring of it.
 * Support for actually consuming a resource (let alone a 'penalize' one)
 * varies by domain and by algorithm within a domain - check the domain's own
 * 'BinType'/'Solution'/algorithms before assuming it is handled everywhere.
 *
 * By default ('penalize == false'), the capacity is a hard constraint: a bin
 * whose consumption exceeds it is infeasible. This is what lets
 * combinatorial cuts (e.g. the rectangle Benders decomposition's no-good
 * cuts and pairwise-incompatibility cuts) be expressed exactly as
 * resources, via 'item_consumptions''s per-copy schedule - see its own doc
 * comment.
 *
 * When 'penalize' is 'true' instead, exceeding the capacity does not make
 * the bin infeasible: 'penalty' is subtracted from the solution's profit the
 * first time (per bin) consumption crosses 'capacity', matching a
 * subset-row-cut-style soft penalty (e.g. a triplet cut: packing at least
 * two of three given item types together in the same bin costs 'penalty').
 */
struct Resource
{
    /** Capacity of the resource. */
    double capacity = 0.0;

    /**
     * Resource consumption schedule of each item type that has one - each
     * entry's 'std::vector<double>' gives the consumption charged for the
     * 'copy'-th (0-indexed) unit of that item type packed in a bin with
     * this resource. A 'copy' past the end of the schedule repeats its
     * last entry (so a length-1 schedule means "the same consumption
     * regardless of how many copies are already packed" - the common
     * case); an 'item_type_id' absent from this list implicitly consumes 0
     * regardless of 'copy'.
     *
     * A non-uniform schedule lets a resource express "at least N copies of
     * this item type" as a plain capacity/consumption row: an all-ones
     * schedule of length N followed by a single trailing 0 makes the total
     * consumption equal to 'min(count, N)', which only keeps growing while
     * count < N.
     *
     * Sparse rather than a plain [item_type_id]-indexed vector because a
     * resource generated as a combinatorial cut (see e.g. the rectangle
     * Benders decomposition's no-good and triplet-incompatibility cuts)
     * typically only involves a handful of item types, yet every consumer
     * that used to index a dense vector by 'item_type_id' had to be sized
     * (and, worse, scanned - see 'onedimensional::milp_assignment.cpp's
     * constraint builder before this) up to the highest item type id
     * referenced anywhere in the instance, however large that is.
     *
     * At most one entry per 'item_type_id' - 'InstanceBuilder::build'
     * throws otherwise (see its own doc comment); 'InstanceBuilder::
     * add_resource_consumption' may therefore be called at most once per
     * (item type, resource) pair, and simply appends here, no search
     * needed. No per-item-type lookup method is offered here on purpose -
     * a one-off search (linear or otherwise) through this list is not
     * O(1), so a caller that already knows which item type it wants would
     * pay for one on every query. Look the position up once instead, via
     * 'ItemType::resources', and index this list directly with it.
     */
    std::vector<std::pair<ItemTypeId, std::vector<double>>> item_consumptions;

    /**
     * If 'true', exceeding 'capacity' does not make a bin infeasible;
     * instead, 'penalty' is subtracted from the solution's profit. See this
     * struct's own doc comment.
     */
    bool penalize = false;

    /** Penalty subtracted from the solution's profit; only used when 'penalize' is 'true'. */
    double penalty = 0.0;

    /**
     * Absolute tolerance used by 'strictly_greater_resource' /
     * 'strictly_lesser_resource' / 'equal_resource' for the comparisons of
     * the consumptions of the resource with its capacity.
     *
     * Computed by 'InstanceBuilder::add_bin_type_resource' from the capacity
     * (see 'compute_resource_tolerance'), so that it is the same in the
     * sub-instances, whose resources are copied with the same capacity - not
     * meant to be set directly.
     */
    double tolerance = 0.0;
};

/**
 * Tolerance of a resource of capacity 'capacity' (see 'Resource::tolerance'):
 * 1e-9 relative to the capacity, or a plain 1e-9 if the capacity is 0.
 */
inline double compute_resource_tolerance(double capacity)
{
    return (capacity != 0.0)?
        std::abs(capacity) * 1e-9:
        1e-9;
}

/**
 * Absolute-tolerance comparisons for resource consumptions, with the
 * tolerance of the resource (see 'Resource::tolerance'), as for the weights
 * (see 'strictly_greater_weight').
 */
inline bool strictly_greater_resource(double v1, double v2, double resource_tolerance)
{
    return v1 - v2 > resource_tolerance;
}

inline bool strictly_lesser_resource(double v1, double v2, double resource_tolerance)
{
    return strictly_greater_resource(v2, v1, resource_tolerance);
}

inline bool equal_resource(double v1, double v2, double resource_tolerance)
{
    return !strictly_greater_resource(v1, v2, resource_tolerance)
        && !strictly_greater_resource(v2, v1, resource_tolerance);
}

/**
 * Result of a domain's 'FeasibilityCallback' invocation (see each domain's
 * own 'instance.hpp' for the callback itself).
 *
 * 'resources' lets the callback hand back its own reason for an infeasible
 * verdict, expressed as one or more combinatorial cuts (see 'Resource'),
 * instead of only a bare 'true'/'false' - so that whichever algorithm owns
 * the callback (e.g. a caller re-solving the same instance repeatedly, one
 * cut at a time) can accumulate and reuse them itself, rather than
 * re-deriving the same infeasibility from scratch on a later call. Empty
 * when 'feasible' is 'true', or when the callback has no cut to offer for
 * an infeasible verdict (a plain 'false', same as before this field
 * existed, is still a completely valid result).
 */
struct FeasibilityCallbackResult
{
    /** 'false' marks the solution the callback was given as infeasible. */
    bool feasible = true;

    /** Cuts the callback derived while proving 'feasible == false', if any. */
    std::vector<Resource> resources;
};

/**
 * One of a bin type's resources a given item type has a non-zero
 * consumption for, together with the position of that item type's own
 * entry in the resource's own (sparse) 'Resource::item_consumptions' list
 * - see 'ItemType::resources' - so that a caller already holding one of
 * these can jump straight to 'bin_type.resource(resource_id).item_consumptions[consumption_pos].second'
 * for the schedule, with no search at all.
 */
struct ItemResourceConsumption
{
    ResourceId resource_id;
    std::size_t consumption_pos;
};

/**
 * Get the consumption of the 'copy'-th (0-indexed) unit of an item type,
 * given its schedule (see 'Resource::item_consumptions') - O(1), unlike a
 * per-item-type lookup would be. 'schedule' is typically obtained via
 * 'ItemType::resources' (see 'ItemResourceConsumption').
 */
inline double schedule_consumption(
        const std::vector<double>& schedule,
        ItemPos copy)
{
    if (schedule.empty())
        return 0.0;
    return (copy < (ItemPos)schedule.size())?
        schedule[copy]:
        schedule.back();
}

/**
 * Return, for each bin type of 'instance' and each item type, 'true' iff the
 * consumption of a copy of the item type of a resource of the bin type
 * depends on the number of copies of the item type already in the bin: its
 * consumption schedule isn't constant.
 *
 * The dominance rules of the tree searches compare the resource
 * consumptions of the last bin of two nodes. This is only valid if the
 * future insertions consume the same quantities in both nodes, which, for
 * these item types, requires the same number of copies in the last bin.
 */
template <typename Instance>
std::vector<std::vector<bool>> copy_dependent_resource_consumptions(
        const Instance& instance)
{
    std::vector<std::vector<bool>> output(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance.bin_type(bin_type_id);
        output[bin_type_id].resize(instance.number_of_item_types(), false);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            for (const auto& item_consumption: bin_type.resource(resource_id).item_consumptions) {
                const std::vector<double>& schedule = item_consumption.second;
                for (double consumption: schedule) {
                    if (consumption != schedule.front()) {
                        output[bin_type_id][item_consumption.first] = true;
                        break;
                    }
                }
            }
        }
    }
    return output;
}

/**
 * Consumption of each resource of bin type 'bin_type_id' by the copies
 * 'item_copies' inserted in a bin which already contains
 * 'item_number_of_copies' copies of each item type.
 *
 * 'consumption' is their consumption alone in the bin, their copies starting
 * at the first entry of the schedules (see 'Resource::item_consumptions').
 * It differs for the item types whose consumption depends on the number of
 * copies already in the bin (see 'copy_dependent_resource_consumptions').
 *
 * Return 'consumption' if it doesn't differ, and 'buffer', filled with the
 * shifted consumption, otherwise.
 */
template <typename Instance>
const std::vector<double>& shifted_resource_consumption(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<bool>& copy_dependent_resource_consumptions,
        const std::vector<ItemPos>& item_number_of_copies,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies,
        const std::vector<double>& consumption,
        std::vector<double>& buffer)
{
    bool shifted = false;
    for (const std::pair<ItemTypeId, ItemPos>& item_copy: item_copies) {
        if (copy_dependent_resource_consumptions[item_copy.first]
                && item_number_of_copies[item_copy.first] > 0) {
            shifted = true;
            break;
        }
    }
    if (!shifted)
        return consumption;

    const auto& bin_type = instance.bin_type(bin_type_id);
    buffer = consumption;
    for (const std::pair<ItemTypeId, ItemPos>& item_copy: item_copies) {
        ItemTypeId item_type_id = item_copy.first;
        ItemPos number_of_copies_in_bin = item_number_of_copies[item_type_id];
        if (!copy_dependent_resource_consumptions[item_type_id]
                || number_of_copies_in_bin == 0) {
            continue;
        }
        const auto& item_type = instance.item_type(item_type_id);
        for (const ItemResourceConsumption& item_resource_consumption: item_type.resources[bin_type_id]) {
            const std::vector<double>& schedule = bin_type.resource(item_resource_consumption.resource_id)
                .item_consumptions[item_resource_consumption.consumption_pos].second;
            for (ItemPos copy = 0; copy < item_copy.second; ++copy) {
                buffer[item_resource_consumption.resource_id]
                    += schedule_consumption(schedule, number_of_copies_in_bin + copy)
                    - schedule_consumption(schedule, copy);
            }
        }
    }
    return buffer;
}

/**
 * Return 'true' iff item types 'item_type_id_1' and 'item_type_id_2' have the
 * same consumption schedule of every resource of every bin type of
 * 'instance' (an item type which doesn't consume a resource has an empty
 * schedule).
 *
 * The tree searches only order the copies of identical item types (see their
 * predecessors) if they consume the resources in the same way: otherwise,
 * packing one before the other may matter.
 */
template <typename Instance>
bool same_resource_consumptions(
        const Instance& instance,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2)
{
    static const std::vector<double> empty_schedule;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance.bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const std::vector<double>* schedule_1 = &empty_schedule;
            const std::vector<double>* schedule_2 = &empty_schedule;
            for (const auto& item_consumption: bin_type.resource(resource_id).item_consumptions) {
                if (item_consumption.first == item_type_id_1)
                    schedule_1 = &item_consumption.second;
                if (item_consumption.first == item_type_id_2)
                    schedule_2 = &item_consumption.second;
            }
            if (*schedule_1 != *schedule_2)
                return false;
        }
    }
    return true;
}

/**
 * Return 'true' iff two nodes of a tree search whose last bins are of type
 * 'bin_type_id' have the same number of copies, in their last bin, of each
 * item type whose resource consumption depends on it (see
 * 'copy_dependent_resource_consumptions').
 */
bool same_copy_dependent_last_bin_copies(
        const std::vector<std::vector<bool>>& copy_dependent_resource_consumptions,
        BinTypeId bin_type_id,
        const std::vector<ItemPos>& last_bin_item_number_of_copies_1,
        const std::vector<ItemPos>& last_bin_item_number_of_copies_2);

/**
 * Consumption of each resource of bin type 'bin_type_id' by the copies
 * 'item_copies' alone in a bin: the copies of an item type start at the
 * first entry of its schedule (see 'Resource::item_consumptions'). See
 * 'shifted_resource_consumption' for their consumption in a bin which
 * already contains copies of their item types.
 *
 * Indexed by resource id. Empty iff the bin type has no resources.
 */
template <typename Instance>
std::vector<double> compute_resource_consumption(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies)
{
    const auto& bin_type = instance.bin_type(bin_type_id);
    std::vector<double> resource_consumption(bin_type.number_of_resources(), 0.0);
    for (const std::pair<ItemTypeId, ItemPos>& item_copy: item_copies) {
        const auto& item_type = instance.item_type(item_copy.first);
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
 * Return 'true' iff adding 'added_consumption' to 'consumption' doesn't
 * exceed the capacity of a resource of bin type 'bin_type_id' which is not
 * penalized.
 *
 * An empty 'consumption' counts as no consumption.
 */
template <typename Instance>
bool resource_feasible(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<double>& consumption,
        const std::vector<double>& added_consumption)
{
    const auto& bin_type = instance.bin_type(bin_type_id);
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (resource.penalize)
            continue;
        double previous_consumption = consumption.empty()?
            0.0: consumption[resource_id];
        if (strictly_greater_resource(
                    previous_consumption + added_consumption[resource_id],
                    resource.capacity,
                    resource.tolerance)) {
            return false;
        }
    }
    return true;
}

/**
 * Penalty of adding 'added_consumption' to 'consumption': the sum of the
 * penalties of the penalized resources of bin type 'bin_type_id' whose
 * capacity is exceeded for the first time, as in the solutions.
 *
 * An empty 'consumption' counts as no consumption.
 */
template <typename Instance>
Profit resource_penalty(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<double>& consumption,
        const std::vector<double>& added_consumption)
{
    const auto& bin_type = instance.bin_type(bin_type_id);
    Profit penalty = 0;
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (!resource.penalize)
            continue;
        double previous_consumption = consumption.empty()?
            0.0: consumption[resource_id];
        if (!strictly_greater_resource(
                    previous_consumption,
                    resource.capacity,
                    resource.tolerance)
                && strictly_greater_resource(
                    previous_consumption + added_consumption[resource_id],
                    resource.capacity,
                    resource.tolerance)) {
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

/**
 * Sum, over all 'penalize' resources with a negative 'penalty', of
 * '-resource.penalty' (i.e. the magnitude of the profit gain each one could
 * contribute if triggered).
 *
 * A negative penalty *increases* the reported profit when its resource's
 * capacity is exceeded (see 'Resource'). A Knapsack bound computed without
 * modeling resources at all (e.g. a closed-form area/weight relaxation) is
 * only a valid upper bound on the profit actually reported once resources
 * are taken into account if this worst-case gain - every negative-penalty
 * resource triggering at once - is added back to it.
 */
Profit negative_penalty_sum(const std::vector<Resource>& resources);

/**
 * Same as 'negative_penalty_sum(const std::vector<Resource>&)', summed over
 * every bin type of 'instance', each weighted by its number of copies: since
 * a 'penalize' resource is tracked per bin (see 'Resource'), each copy of a
 * bin type can trigger it independently, so the worst case scales with the
 * number of available copies.
 */
template <typename Instance>
inline Profit negative_penalty_sum(const Instance& instance)
{
    Profit sum = 0.0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance.bin_type(bin_type_id);
        sum += bin_type.copies * negative_penalty_sum(bin_type.resources);
    }
    return sum;
}

}
