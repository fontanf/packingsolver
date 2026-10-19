#include "packingsolver/boxstacks/reduction.hpp"

#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/solution_builder.hpp"
#include "algorithms/subset_sum_bound.hpp"
#include "algorithms/dominated_items.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::boxstacks;

bool Reduction::remove_negative_profit_items_applies(const Instance& instance)
{
    return instance.objective() == Objective::Knapsack;
}

ItemPos Reduction::effective_copies_min(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id) const
{
    const ItemType& original_item_type = original_instance_->item_type(item_type_id);
    if (original_instance_->objective() == Objective::Knapsack) {
        return std::min(
                original_item_type.copies_min,
                reduction_item_types[item_type_id].copies);
    }
    return original_item_type.copies_min;
}

namespace
{

/**
 * The 'x' and 'y' of 'box' rotated with 'rotation', which is 'XYZ' or 'YXZ'
 * (items are packed upright).
 */
std::pair<Length, Length> rotated_footprint(
        const Box& box,
        Rotation rotation)
{
    if (rotation == Rotation::YXZ)
        return {box.y, box.x};
    return {box.x, box.y};
}

}

bool Reduction::remove_redundant_rotations(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        const ItemType& item_type = original_instance_->item_type(item_type_id);
        if (item_type.copies_fixed > 0)
            continue;

        // Keep the first rotation of each rotated footprint.
        std::vector<Rotation> distinct_rotations;
        for (Rotation rotation: item.rotations) {
            bool duplicate = false;
            for (Rotation distinct_rotation: distinct_rotations) {
                if (rotated_footprint(item.box, distinct_rotation)
                        == rotated_footprint(item.box, rotation)) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                distinct_rotations.push_back(rotation);
        }

        // Remove the rotations whose rotated footprint fits in no bin type,
        // unless none fits.
        std::vector<Rotation> fitting_rotations;
        for (Rotation rotation: distinct_rotations) {
            std::pair<Length, Length> footprint = rotated_footprint(item.box, rotation);
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < original_instance_->number_of_bin_types();
                    ++bin_type_id) {
                const Box& bin_box = original_instance_->bin_type(bin_type_id).box;
                if (footprint.first <= bin_box.x
                        && footprint.second <= bin_box.y) {
                    fitting_rotations.push_back(rotation);
                    break;
                }
            }
        }
        if (!fitting_rotations.empty())
            distinct_rotations = fitting_rotations;

        if (distinct_rotations.size() != item.rotations.size()) {
            item.rotations = distinct_rotations;
            found = true;
        }
    }
    return found;
}

bool Reduction::remove_negative_profit_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        if (original_instance_->item_type(item_type_id).profit >= 0)
            continue;
        ItemPos min_copies = effective_copies_min(reduction_item_types, item_type_id);
        if (item.copies <= min_copies) {
            // Already at (or, degenerately, past) its own minimum -
            // nothing optional left to trim.
            continue;
        }
        item.copies = min_copies;
        if (item.copies == 0)
            item.removed = true;
        found = true;
    }
    return found;
}

namespace
{

/**
 * 'true' iff 'rotations_1' and 'rotations_2' allow exactly the same set of
 * rotations, regardless of order (order never matters - 'ItemType::
 * can_rotate' just scans the list).
 */
bool same_rotations(
        std::vector<Rotation> rotations_1,
        std::vector<Rotation> rotations_2)
{
    if (rotations_1.size() != rotations_2.size())
        return false;
    std::sort(rotations_1.begin(), rotations_1.end());
    std::sort(rotations_2.begin(), rotations_2.end());
    return rotations_1 == rotations_2;
}

}

bool Reduction::remove_dominated_items_applies(const Instance& instance)
{
    if (instance.objective() != Objective::Knapsack)
        return false;
    // A lighter item can move the center of gravity and increase an axle
    // weight.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        if (instance.bin_type(bin_type_id).semi_trailer_truck_data.is)
            return false;
    }
    return true;
}

bool Reduction::remove_dominated_bin_types_applies(const Instance& instance)
{
    // Only 'Knapsack', 'Feasibility' and 'VariableSizedBinPacking': with
    // the bin packing objectives ('BinPacking', 'BinPackingWithLeftovers',
    // 'BinPackingCuttingCost'), the bin types must be used in the order
    // they are declared, so removing one would make the later ones
    // available earlier than the original instance allows.
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility
            && instance.objective() != Objective::VariableSizedBinPacking) {
        return false;
    }
    return instance.number_of_bin_types() > 1;
}

bool Reduction::item_type_dominates(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_a,
        ItemTypeId item_type_id_b) const
{
    if (item_type_id_a == item_type_id_b)
        return false;
    const ItemType& item_type_a = original_instance_->item_type(item_type_id_a);
    const ItemType& item_type_b = original_instance_->item_type(item_type_id_b);
    if (item_type_a.profit < item_type_b.profit)
        return false;
    if (item_type_a.copies_fixed > 0 || item_type_b.copies_fixed > 0)
        return false;
    if (item_type_a.stackability_id != item_type_b.stackability_id)
        return false;
    if (item_type_a.group_id != item_type_b.group_id)
        return false;
    if (item_type_a.weight > item_type_b.weight)
        return false;
    if (item_type_a.maximum_weight_above < item_type_b.maximum_weight_above)
        return false;
    if (item_type_a.maximum_stackability < item_type_b.maximum_stackability)
        return false;
    if (item_type_a.box.z > item_type_b.box.z)
        return false;
    if (item_type_a.box.z - item_type_a.nesting_height
            > item_type_b.box.z - item_type_b.nesting_height) {
        return false;
    }
    // A takes the place of B in its stack: same footprint, whatever the
    // rotation of B.
    for (Rotation rotation_b: reduction_item_types[item_type_id_b].rotations) {
        std::pair<Length, Length> footprint_b = rotated_footprint(item_type_b.box, rotation_b);
        bool found = false;
        for (Rotation rotation_a: reduction_item_types[item_type_id_a].rotations) {
            if (rotated_footprint(item_type_a.box, rotation_a) == footprint_b) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

bool Reduction::items_incompatible(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2,
        BinTypeId bin_type_id) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);
    const BinType& bin_type = original_instance_->bin_type(bin_type_id);
    if (item_type_1.weight + item_type_2.weight > bin_type.maximum_weight)
        return true;
    // They may share a stack.
    if (item_type_1.stackability_id == item_type_2.stackability_id)
        return false;
    // Otherwise, their stacks are side by side along 'x' or 'y'.
    for (Rotation rotation_1: reduction_item_types[item_type_id_1].rotations) {
        std::pair<Length, Length> footprint_1 = rotated_footprint(item_type_1.box, rotation_1);
        if (footprint_1.first > bin_type.box.x || footprint_1.second > bin_type.box.y)
            continue;
        for (Rotation rotation_2: reduction_item_types[item_type_id_2].rotations) {
            std::pair<Length, Length> footprint_2 = rotated_footprint(item_type_2.box, rotation_2);
            if (footprint_2.first > bin_type.box.x || footprint_2.second > bin_type.box.y)
                continue;
            if (footprint_1.first + footprint_2.first <= bin_type.box.x
                    || footprint_1.second + footprint_2.second <= bin_type.box.y) {
                return false;
            }
        }
    }
    return true;
}

ItemPos Reduction::dominated_item_copies_bound(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_a,
        ItemTypeId item_type_id_b) const
{
    const ItemType& item_type_a = original_instance_->item_type(item_type_id_a);
    const ItemType& item_type_b = original_instance_->item_type(item_type_id_b);
    // A takes at least this volume, nested or not; and B at least as much.
    Volume volume_a = item_type_a.box.x * item_type_a.box.y
        * (item_type_a.box.z - item_type_a.nesting_height);
    if (volume_a <= 0)
        return reduction_item_types[item_type_id_b].copies;
    auto fits = [this, &reduction_item_types](
            ItemTypeId item_type_id,
            const BinType& bin_type)
    {
        const ItemType& item_type = original_instance_->item_type(item_type_id);
        if (item_type.box.z > bin_type.box.z)
            return false;
        for (Rotation rotation: reduction_item_types[item_type_id].rotations) {
            std::pair<Length, Length> footprint = rotated_footprint(item_type.box, rotation);
            if (footprint.first <= bin_type.box.x && footprint.second <= bin_type.box.y)
                return true;
        }
        return false;
    };
    std::vector<DominatedItemsBinType> bin_types;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        DominatedItemsBinType dominated_items_bin_type;
        dominated_items_bin_type.copies = bin_type.copies;
        dominated_items_bin_type.fits_b = fits(item_type_id_b, bin_type);
        bool fits_a = fits(item_type_id_a, bin_type);
        if (fits_a || dominated_items_bin_type.fits_b) {
            Counter maximum_number_of_items = bin_type.box.volume() / volume_a;
            bool incompatible_a_a = items_incompatible(
                    reduction_item_types, item_type_id_a, item_type_id_a, bin_type_id);
            dominated_items_bin_type.exclusive
                = items_incompatible(reduction_item_types, item_type_id_a, item_type_id_b, bin_type_id)
                && items_incompatible(reduction_item_types, item_type_id_b, item_type_id_b, bin_type_id);
            dominated_items_bin_type.maximum_number_of_copies_a = (!fits_a)? 0:
                (incompatible_a_a)? 1: maximum_number_of_items;
            dominated_items_bin_type.maximum_number_of_items
                = (incompatible_a_a && dominated_items_bin_type.exclusive)?
                1: maximum_number_of_items;
        }
        bin_types.push_back(dominated_items_bin_type);
    }
    return (ItemPos)packingsolver::dominated_item_copies_bound(
            reduction_item_types[item_type_id_a].copies,
            reduction_item_types[item_type_id_b].copies,
            bin_types);
}

bool Reduction::remove_dominated_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (ItemTypeId item_type_id_b = 0;
            item_type_id_b < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_b) {
        if (reduction_item_types[item_type_id_b].removed)
            continue;
        if (effective_copies_min(reduction_item_types, item_type_id_b) != 0)
            continue;
        for (ItemTypeId item_type_id_a = 0;
                item_type_id_a < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_a) {
            if (item_type_id_a == item_type_id_b)
                continue;
            if (reduction_item_types[item_type_id_a].removed)
                continue;
            if (!item_type_dominates(reduction_item_types, item_type_id_a, item_type_id_b))
                continue;
            ItemPos bound = dominated_item_copies_bound(
                    reduction_item_types, item_type_id_a, item_type_id_b);
            ReductionItemType& item_b = reduction_item_types[item_type_id_b];
            if (bound >= item_b.copies)
                continue;
            item_b.copies = bound;
            if (item_b.copies == 0)
                item_b.removed = true;
            found = true;
            if (item_b.removed)
                break;
        }
    }
    return found;
}

bool Reduction::bin_type_dominates(
        BinTypeId bin_type_id_a,
        BinTypeId bin_type_id_b) const
{
    if (bin_type_id_a == bin_type_id_b)
        return false;
    const BinType& bin_type_a = original_instance_->bin_type(bin_type_id_a);
    const BinType& bin_type_b = original_instance_->bin_type(bin_type_id_b);
    // A must be able to hold anything B could.
    if (bin_type_a.box.x < bin_type_b.box.x
            || bin_type_a.box.y < bin_type_b.box.y
            || bin_type_a.box.z < bin_type_b.box.z) {
        return false;
    }
    if (bin_type_a.cost > bin_type_b.cost)
        return false;
    if (bin_type_a.maximum_weight < bin_type_b.maximum_weight)
        return false;
    if (bin_type_a.maximum_stack_density < bin_type_b.maximum_stack_density)
        return false;
    if (!bin_type_a.defects.empty() || !bin_type_b.defects.empty())
        return false;
    if (!bin_type_a.fixed_items.empty() || !bin_type_b.fixed_items.empty())
        return false;
    if (bin_type_a.semi_trailer_truck_data.is || bin_type_b.semi_trailer_truck_data.is)
        return false;
    return true;
}

std::vector<bool> Reduction::compute_dominated_bin_types() const
{
    std::vector<bool> dominated(original_instance_->number_of_bin_types(), false);
    BinPos number_of_items = original_instance_->number_of_items();
    for (BinTypeId bin_type_id_b = 0;
            bin_type_id_b < original_instance_->number_of_bin_types();
            ++bin_type_id_b) {
        // A bin type that must be used can not be replaced.
        if (original_instance_->bin_type(bin_type_id_b).copies_min != 0)
            continue;
        for (BinTypeId bin_type_id_a = 0;
                bin_type_id_a < original_instance_->number_of_bin_types();
                ++bin_type_id_a) {
            if (bin_type_id_a == bin_type_id_b)
                continue;
            if (dominated[bin_type_id_a])
                continue;
            // A needs enough copies to replace every bin of B a solution
            // could use, on top of its own mandatory ones: a solution never
            // needs more non-empty bins than items.
            const BinType& bin_type_a = original_instance_->bin_type(bin_type_id_a);
            if (bin_type_a.copies < number_of_items + bin_type_a.copies_min)
                continue;
            if (!bin_type_dominates(bin_type_id_a, bin_type_id_b))
                continue;
            dominated[bin_type_id_b] = true;
            break;
        }
    }
    return dominated;
}

bool Reduction::lift_item_dimensions_applies(const Instance& instance)
{
    // A single bin type, so that the bin dimensions are unambiguous, and
    // no defects or fixed items, which the compaction argument of the
    // class-level doc comment cannot move.
    return instance.number_of_bin_types() == 1
        && instance.bin_type(0).defects.empty()
        && instance.bin_type(0).fixed_items.empty();
}

bool Reduction::lift_item_dimensions_applies_axis(
        const Instance& instance,
        int axis)
{
    // See the class-level doc comment for each exclusion.
    rectangle::UnloadingConstraint unloading_constraint = instance.unloading_constraint();
    LeftoverMode leftover_mode = instance.parameters().leftover_mode;
    if (axis == 0) {
        if (instance.objective() == Objective::OpenDimensionX)
            return false;
        if (instance.objective() == Objective::BinPackingWithLeftovers
                && (leftover_mode == LeftoverMode::X
                    || leftover_mode == LeftoverMode::XY)) {
            return false;
        }
        if (instance.bin_type(0).semi_trailer_truck_data.is)
            return false;
        if (unloading_constraint == rectangle::UnloadingConstraint::OnlyYMovements
                || unloading_constraint == rectangle::UnloadingConstraint::IncreasingX) {
            return false;
        }
    } else if (axis == 1) {
        if (instance.objective() == Objective::OpenDimensionY)
            return false;
        if (instance.objective() == Objective::BinPackingWithLeftovers
                && (leftover_mode == LeftoverMode::Y
                    || leftover_mode == LeftoverMode::XY)) {
            return false;
        }
        if (unloading_constraint == rectangle::UnloadingConstraint::OnlyXMovements
                || unloading_constraint == rectangle::UnloadingConstraint::IncreasingY) {
            return false;
        }
    } else {
        if (instance.objective() == Objective::OpenDimensionZ)
            return false;
    }
    return true;
}

Length Reduction::max_achievable_dimension_sum(
        const std::vector<ReductionItemType>& reduction_item_types,
        Length capacity,
        int axis,
        ItemTypeId excluded_item_type_id) const
{
    std::vector<SubsetSumBoundItem> items;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        SubsetSumBoundItem subset_sum_item;
        subset_sum_item.copies = item.copies;
        if (item_type_id == excluded_item_type_id)
            subset_sum_item.copies--;
        for (Rotation rotation: item.rotations) {
            std::pair<Length, Length> footprint = rotated_footprint(item.box, rotation);
            subset_sum_item.lengths.push_back((axis == 0)? footprint.first: footprint.second);
        }
        items.push_back(subset_sum_item);
    }
    return subset_sum_bound(items, capacity);
}

Length Reduction::shrunk_bin_length(
        const std::vector<ReductionItemType>& reduction_item_types,
        int axis) const
{
    const Box& bin_box = original_instance_->bin_type(0).box;
    return max_achievable_dimension_sum(
            reduction_item_types,
            (axis == 0)? bin_box.x: bin_box.y,
            axis,
            -1);
}

bool Reduction::lift_item_dimensions(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    const BinType& bin_type = original_instance_->bin_type(0);
    bool found = false;
    for (int axis = 0; axis < 3; ++axis) {
        if (!lift_item_dimensions_applies_axis(*original_instance_, axis))
            continue;

        // Lifting an item type never changes the shrunk bin: every
        // combination of items containing the enlarged item type still
        // fits in it.
        Length bin_length = (axis == 2)?
            bin_type.box.z:
            shrunk_bin_length(reduction_item_types, axis);

        for (ItemTypeId item_type_id = 0;
                item_type_id < (ItemTypeId)reduction_item_types.size();
                ++item_type_id) {
            ReductionItemType& item = reduction_item_types[item_type_id];
            if (item.removed || item.rotations.empty())
                continue;
            // A single copy: the bound below only holds for one occurrence
            // of the item type (see
            // 'rectangle::Reduction::lift_item_dimensions_axis').
            if (item.copies != 1)
                continue;
            const ItemType& item_type = original_instance_->item_type(item_type_id);
            if (item_type.copies_fixed > 0)
                continue;

            // Alone in its stack: no other remaining item type shares its
            // stackability id.
            bool alone = true;
            for (ItemTypeId other_item_type_id = 0;
                    other_item_type_id < (ItemTypeId)reduction_item_types.size();
                    ++other_item_type_id) {
                if (other_item_type_id == item_type_id
                        || reduction_item_types[other_item_type_id].removed) {
                    continue;
                }
                if (original_instance_->item_type(other_item_type_id).stackability_id
                        == item_type.stackability_id) {
                    alone = false;
                    break;
                }
            }
            if (!alone)
                continue;

            if (axis == 2) {
                // Nothing else is ever packed in its stack.
                if (item.box.z < bin_length) {
                    item.box.z = bin_length;
                    found = true;
                }
                continue;
            }

            // A single rotation, so that growing 'x' (or 'y') only changes
            // the extent of the item along this axis.
            if (item.rotations.size() != 1)
                continue;
            // The maximum stack density is checked by the reduced instance
            // against the enlarged footprint: the item must satisfy it with
            // its true footprint.
            if (strictly_greater(
                        (double)item_type.weight / item_type.box.x / item_type.box.y,
                        bin_type.maximum_stack_density)) {
                continue;
            }

            Rotation rotation = item.rotations.front();
            // The dimension of the box along the axis for this rotation.
            bool along_x = ((axis == 0) == (rotation != Rotation::YXZ));
            Length& item_length = (along_x)? item.box.x: item.box.y;
            Length capacity = bin_length - item_length;
            if (capacity <= 0)
                continue;
            Length achievable_other = max_achievable_dimension_sum(
                    reduction_item_types, capacity, axis, item_type_id);
            if (achievable_other >= capacity)
                continue;
            item_length = bin_length - achievable_other;
            found = true;
        }
    }
    return found;
}

bool Reduction::items_mergeable(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);

    // Current boxes: see 'lift_item_dimensions'.
    const Box& box_1 = reduction_item_types[item_type_id_1].box;
    const Box& box_2 = reduction_item_types[item_type_id_2].box;
    if (box_1.x != box_2.x
            || box_1.y != box_2.y
            || box_1.z != box_2.z)
        return false;

    // Profit is only compared for 'Knapsack': for every other objective
    // profit is never the actual objective, and 'unreduce_solution' always
    // restores the true original item type id (and so its true profit) for
    // every placed copy regardless of which one a reduced-instance solve
    // actually used - but 'Knapsack' optimizes profit directly over a solve
    // that may legitimately leave copies unplaced, so merging two
    // different-profit item types would report one uniform profit for
    // every copy and let the solve itself optimize against wrong values.
    if (original_instance_->objective() == Objective::Knapsack
            && item_type_1.profit != item_type_2.profit)
        return false;

    if (!same_rotations(
                reduction_item_types[item_type_id_1].rotations,
                reduction_item_types[item_type_id_2].rotations))
        return false;
    if (item_type_1.weight != item_type_2.weight)
        return false;
    if (item_type_1.group_id != item_type_2.group_id)
        return false;
    if (item_type_1.stackability_id != item_type_2.stackability_id)
        return false;
    if (item_type_1.nesting_height != item_type_2.nesting_height)
        return false;
    if (item_type_1.maximum_stackability != item_type_2.maximum_stackability)
        return false;
    if (item_type_1.maximum_weight_above != item_type_2.maximum_weight_above)
        return false;

    // Each candidate's own current copies must be either entirely
    // mandatory or entirely optional, and both candidates must agree on
    // which - see this method's own doc comment in 'reduction.hpp'
    // (mirrors 'rectangle::Reduction::items_mergeable''s own, more detailed
    // comment for the full counterexample this guards against).
    const ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
    const ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
    ItemPos effective_copies_min_1 = effective_copies_min(reduction_item_types, item_type_id_1);
    ItemPos effective_copies_min_2 = effective_copies_min(reduction_item_types, item_type_id_2);
    bool item_1_fully_mandatory = (effective_copies_min_1 == item_1.copies);
    bool item_2_fully_mandatory = (effective_copies_min_2 == item_2.copies);
    bool item_1_fully_optional = (effective_copies_min_1 == 0);
    bool item_2_fully_optional = (effective_copies_min_2 == 0);
    if (!(item_1_fully_mandatory || item_1_fully_optional)
            || !(item_2_fully_mandatory || item_2_fully_optional))
        return false;
    if (item_1_fully_mandatory != item_2_fully_mandatory)
        return false;

    return true;
}

bool Reduction::merge_identical_items(
        std::vector<ReductionItemType>& reduction_item_types)
{
    bool found = false;
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_1) {
        if (reduction_item_types[item_type_id_1].removed)
            continue;
        for (ItemTypeId item_type_id_2 = item_type_id_1 + 1;
                item_type_id_2 < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_2) {
            if (reduction_item_types[item_type_id_2].removed)
                continue;
            if (!items_mergeable(reduction_item_types, item_type_id_1, item_type_id_2))
                continue;
            reduction_item_types[item_type_id_2].removed = true;
            reduction_item_types[item_type_id_2].merged_into = item_type_id_1;
            found = true;
        }
    }
    return found;
}

Instance Reduction::reduction_to_instance(
        const std::vector<ReductionItemType>& reduction_item_types,
        const std::vector<bool>& bin_type_removed)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(original_instance_->objective());
    instance_builder.set_parameters(original_instance_->parameters());
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    reduced_to_original_bin_type_id_.clear();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        if (bin_type_id < (BinTypeId)bin_type_removed.size()
                && bin_type_removed[bin_type_id]) {
            continue;
        }
        instance_builder.add_bin_type(*original_instance_, bin_type_id);
        reduced_to_original_bin_type_id_.push_back(bin_type_id);
    }

    reduced_item_origin_runs_.clear();
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;

        // Origin runs for this survivor: its own copies, then, in
        // discovery order, one more run per item type merged into it (see
        // 'merge_identical_items') - any consistent order works, since
        // merged item types are interchangeable by construction (see
        // 'items_mergeable').
        std::vector<OriginRun> origin_runs;
        ItemPos total_copies = item.copies;
        ItemPos total_copies_min = effective_copies_min(reduction_item_types, item_type_id);
        if (item.copies > 0)
            origin_runs.push_back({item_type_id, item.copies});
        for (ItemTypeId other_item_type_id = 0;
                other_item_type_id < (ItemTypeId)reduction_item_types.size();
                ++other_item_type_id) {
            if (reduction_item_types[other_item_type_id].merged_into != item_type_id)
                continue;
            ItemPos merged_copies = reduction_item_types[other_item_type_id].copies;
            total_copies += merged_copies;
            total_copies_min += effective_copies_min(reduction_item_types, other_item_type_id);
            if (merged_copies > 0)
                origin_runs.push_back({other_item_type_id, merged_copies});
        }

        ItemTypeId new_item_type_id = instance_builder.add_item_type(*original_instance_, item_type_id);
        instance_builder.set_item_type_copies(new_item_type_id, total_copies);
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
        instance_builder.set_item_type_rotations(new_item_type_id, item.rotations);
        instance_builder.set_item_type_box(new_item_type_id, item.box);
        reduced_item_origin_runs_.push_back(std::move(origin_runs));
    }

    return instance_builder.build();
}

Reduction::Reduction(
        const Instance& instance,
        const ReductionParameters& parameters):
    original_instance_(&instance),
    instance_(instance)
{
    if (!parameters.reduce)
        return;

    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
        reduction_item_types[item_type_id].rotations = instance.item_type(item_type_id).rotations;
        reduction_item_types[item_type_id].box = instance.item_type(item_type_id).box;
    }

    if (parameters.remove_redundant_rotations)
        remove_redundant_rotations(reduction_item_types);

    if (parameters.remove_negative_profit_items
            && remove_negative_profit_items_applies(instance)) {
        remove_negative_profit_items(reduction_item_types);
    }

    // Before lifting: the dominance compares the original boxes.
    if (parameters.remove_dominated_items
            && remove_dominated_items_applies(instance)) {
        remove_dominated_items(reduction_item_types);
    }

    // Lifting can make more rotations redundant (e.g. a square enlarged
    // footprint), and removing rotations can enable lifting along 'x' and
    // 'y' (it requires a single allowed rotation).
    if (parameters.lift_item_dimensions
            && lift_item_dimensions_applies(instance)) {
        for (;;) {
            if (parameters.timer.needs_to_end())
                break;
            if (!lift_item_dimensions(reduction_item_types))
                break;
            if (!parameters.remove_redundant_rotations
                    || !remove_redundant_rotations(reduction_item_types)) {
                break;
            }
        }
    }

    if (parameters.merge_identical_items)
        merge_identical_items(reduction_item_types);

    std::vector<bool> bin_type_removed;
    if (parameters.remove_dominated_bin_types
            && remove_dominated_bin_types_applies(instance)) {
        bin_type_removed = compute_dominated_bin_types();
    }

    instance_ = reduction_to_instance(reduction_item_types, bin_type_removed);
}

ItemTypeId Reduction::consume_one_origin(
        std::vector<OriginRun>& runs)
{
    ItemTypeId original_item_type_id = runs.back().item_type_id;
    if (--runs.back().count == 0)
        runs.pop_back();
    return original_item_type_id;
}

Solution Reduction::unreduce_solution(
        const Solution& solution) const
{
    SolutionBuilder solution_builder(*original_instance_);

    // A working copy of 'reduced_item_origin_runs_' that this call
    // consumes from (via 'consume_one_origin') as it scans 'solution' -
    // kept local rather than a member so nothing here needs resetting
    // between (or interferes with) separate 'unreduce_solution' calls.
    std::vector<std::vector<OriginRun>> remaining_runs = reduced_item_origin_runs_;

    // Footprint of a stack with the true dimensions of its items: a stack
    // containing a lifted item type contains nothing else (see
    // 'lift_item_dimensions'), and every item type merged into another one
    // has the same original box (a lifted item type is never merged), so
    // the original box of any origin of the first item gives it.
    auto true_footprint = [this](const SolutionStack& solution_stack)
        -> std::pair<Length, Length>
    {
        if (solution_stack.items.empty()) {
            return {
                solution_stack.x_end - solution_stack.x_start,
                solution_stack.y_end - solution_stack.y_start};
        }
        const SolutionItem& solution_item = solution_stack.items.front();
        ItemTypeId original_item_type_id
            = reduced_item_origin_runs_[solution_item.item_type_id].front().item_type_id;
        return rotated_footprint(
                original_instance_->item_type(original_item_type_id).box,
                solution_item.rotation);
    };

    // How many of the *current* pattern's item slots (across every
    // stack) reference each reduced item type - needed to tell whether a
    // whole physical bin's worth of a given type can still be drawn from
    // its current run without crossing into the next one (see below).
    // Declared once, outside the bin loop, and only ever touched at the
    // (few) item type ids each pattern actually uses - 'touched_types'
    // both drives that targeted reset and stands in for the
    // distinct-types set itself, so nothing here needs a 'std::map'.
    std::vector<ItemPos> slot_count(instance_.number_of_item_types(), 0);
    std::vector<ItemTypeId> touched_types;

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);

        touched_types.clear();
        for (const SolutionStack& solution_stack: solution_bin.stacks) {
            for (const SolutionItem& solution_item: solution_stack.items) {
                if (slot_count[solution_item.item_type_id] == 0)
                    touched_types.push_back(solution_item.item_type_id);
                slot_count[solution_item.item_type_id]++;
            }
        }

        BinPos remaining_bin_copies = solution_bin.copies;
        while (remaining_bin_copies > 0) {
            // 'true' iff every reduced item type this pattern places can
            // supply this whole bin's worth of copies from its *current*
            // run alone. Whenever this holds, an entire batch of
            // consecutive physical bins - not just this one - can share a
            // single 'add_bin' entry (see below); the common case (a type
            // that was never merged at all has only one run ever, so this
            // is always 'true' for it) is what keeps this whole loop down
            // to a handful of iterations, regardless of how many physical
            // copies there actually are.
            bool bin_fits_in_current_runs = true;
            for (ItemTypeId reduced_item_type_id: touched_types) {
                if (remaining_runs[reduced_item_type_id].back().count
                        < slot_count[reduced_item_type_id]) {
                    bin_fits_in_current_runs = false;
                    break;
                }
            }

            if (bin_fits_in_current_runs) {
                // Every slot's origin is fixed for as long as this batch
                // lasts (each type stays on its current run throughout) -
                // resolve it once, then compute the largest batch of
                // consecutive physical bins that can share it: the
                // tightest of every placed type's own
                // 'floor(remaining in its current run / copies needed per
                // bin)'.
                BinPos batch = remaining_bin_copies;
                for (ItemTypeId reduced_item_type_id: touched_types) {
                    batch = std::min(
                            batch,
                            (BinPos)(remaining_runs[reduced_item_type_id].back().count
                                / slot_count[reduced_item_type_id]));
                }
                BinPos new_bin_pos = solution_builder.add_bin(reduced_to_original_bin_type_id_[solution_bin.bin_type_id], batch);
                for (const SolutionStack& solution_stack: solution_bin.stacks) {
                    std::pair<Length, Length> footprint = true_footprint(solution_stack);
                    StackId new_stack_id = solution_builder.add_stack(
                            new_bin_pos,
                            solution_stack.x_start,
                            solution_stack.x_start + footprint.first,
                            solution_stack.y_start,
                            solution_stack.y_start + footprint.second);
                    for (const SolutionItem& solution_item: solution_stack.items) {
                        ItemTypeId original_item_type_id
                            = remaining_runs[solution_item.item_type_id].back().item_type_id;
                        solution_builder.add_item(
                                new_bin_pos,
                                new_stack_id,
                                original_item_type_id,
                                solution_item.rotation);
                    }
                }
                for (ItemTypeId reduced_item_type_id: touched_types) {
                    OriginRun& run = remaining_runs[reduced_item_type_id].back();
                    run.count -= slot_count[reduced_item_type_id] * batch;
                    if (run.count == 0)
                        remaining_runs[reduced_item_type_id].pop_back();
                }
                remaining_bin_copies -= batch;
            } else {
                // At least one placed type would run out mid-bin - resolve
                // this one physical bin the slow way instead (still only
                // ever as many calls as this one bin has item slots, not
                // proportional to 'remaining_bin_copies').
                BinPos new_bin_pos = solution_builder.add_bin(reduced_to_original_bin_type_id_[solution_bin.bin_type_id], 1);
                for (const SolutionStack& solution_stack: solution_bin.stacks) {
                    std::pair<Length, Length> footprint = true_footprint(solution_stack);
                    StackId new_stack_id = solution_builder.add_stack(
                            new_bin_pos,
                            solution_stack.x_start,
                            solution_stack.x_start + footprint.first,
                            solution_stack.y_start,
                            solution_stack.y_start + footprint.second);
                    for (const SolutionItem& solution_item: solution_stack.items) {
                        ItemTypeId original_item_type_id
                            = consume_one_origin(remaining_runs[solution_item.item_type_id]);
                        solution_builder.add_item(
                                new_bin_pos,
                                new_stack_id,
                                original_item_type_id,
                                solution_item.rotation);
                    }
                }
                remaining_bin_copies--;
            }
        }

        for (ItemTypeId reduced_item_type_id: touched_types)
            slot_count[reduced_item_type_id] = 0;
    }

    return solution_builder.build();
}
