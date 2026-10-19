#include "packingsolver/rectangleguillotine/reduction.hpp"

#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/solution_builder.hpp"
#include "algorithms/subset_sum_bound.hpp"
#include "algorithms/dominated_items.hpp"

#include <stdexcept>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

/**
 * The consumption schedule 'item_type' has for 'resource_id' in bin type
 * 'bin_type_id' of 'bin_type' (see 'ItemType::resources'), or 'nullptr' if
 * it has none there. Same helper (same name, same semantics) as
 * 'rectangle::reduction.cpp''s own local one, reimplemented here since it
 * is not shared/exported.
 */
const std::vector<double>* find_item_type_schedule(
        const ItemType& item_type,
        BinTypeId bin_type_id,
        const BinType& bin_type,
        ResourceId resource_id)
{
    for (const ItemResourceConsumption& entry: item_type.resources[bin_type_id]) {
        if (entry.resource_id == resource_id)
            return &bin_type.resource(resource_id).item_consumptions[entry.consumption_pos].second;
    }
    return nullptr;
}

}

bool Reduction::remove_negative_profit_items_applies(const Instance& instance)
{
    if (instance.objective() != Objective::Knapsack)
        return false;

    // See this class's own doc comment for why a 'penalize' resource with a
    // negative 'penalty' blocks this operation.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            if (resource.penalize && resource.penalty < 0)
                return false;
        }
    }
    return true;
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
    ItemPos consumed = original_item_type.copies - reduction_item_types[item_type_id].copies;
    return std::max((ItemPos)0, original_item_type.copies_min - consumed);
}

bool Reduction::remove_redundant_rotations(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed
                || item.oriented
                || original_instance_->item_type(item_type_id).copies_fixed > 0) {
            continue;
        }
        if (item.rect.w == item.rect.h) {
            item.oriented = true;
            found = true;
            continue;
        }
        bool fits = false;
        bool fits_rotated = false;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < original_instance_->number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = original_instance_->bin_type(bin_type_id);
            Length width = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
            Length height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
            if (item.rect.w <= width && item.rect.h <= height)
                fits = true;
            if (item.rect.h <= width && item.rect.w <= height)
                fits_rotated = true;
        }
        if (fits && !fits_rotated) {
            item.oriented = true;
            found = true;
        } else if (!fits && fits_rotated) {
            item.oriented = true;
            std::swap(item.rect.w, item.rect.h);
            item.swapped = true;
            found = true;
        }
    }
    return found;
}

void Reduction::remove_negative_profit_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        const ItemType& original_item_type = original_instance_->item_type(item_type_id);
        if (original_item_type.profit >= 0)
            continue;
        // Leave item types sharing a stack with others untouched - see
        // 'items_mergeable''s own doc comment for why a shared stack's
        // ordering can be silently disturbed. Trimming (or fully removing)
        // a stack-sharing item type's copies here is just as unsound as
        // merging one would be: 'Solution::stacks_feasible_' requires every
        // copy of the item type immediately preceding a given stack
        // position to be fully placed before that position can be used -
        // shrinking (or deleting) this item type's own copy count in the
        // reduced instance silently weakens or erases that requirement for
        // whichever item type comes right after it in the same stack,
        // exactly the same hazard 'items_mergeable' already guards against.
        // An item type alone in its own stack has no such requirement (see
        // 'items_mergeable''s own doc comment), so it is always safe to
        // trim. "Alone" means no *other* item type contributes to the same
        // stack - not that the stack holds exactly one physical item:
        // 'stack_size' counts every physical copy in the stack (see its own
        // doc comment), so this item type's own original copies must
        // account for all of it; comparing against '1' instead would wrongly
        // block this on any alone-in-its-own-stack item type with more than
        // one copy.
        if (original_instance_->stack_size(original_item_type.stack_id) != original_item_type.copies)
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
    }
}

bool Reduction::lift_item_dimensions_applies(const Instance& instance)
{
    // Objectives only depending on which items and bins are used (see the
    // class-level doc comment); the open dimension ones only lift along the
    // other axis (see 'lift_item_dimensions_applies_axis').
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::BinPacking
            && instance.objective() != Objective::VariableSizedBinPacking
            && instance.objective() != Objective::Feasibility
            && instance.objective() != Objective::OpenDimensionX
            && instance.objective() != Objective::OpenDimensionY) {
        return false;
    }
    if (instance.number_of_bin_types() != 1)
        return false;
    const BinType& bin_type = instance.bin_type(0);
    if (!bin_type.fixed_items.empty())
        return false;
    // 'unreduce_solution' shrinks the enlarged items in their nodes. The
    // compaction can also make a distance between two cuts or a waste
    // smaller, which 'items_can_shrink' rules out too.
    if (!items_can_shrink(instance))
        return false;
    // With a cut thickness, the extent of a chain of items next to a trim
    // depends on whether the trim is cut.
    const Parameters& parameters = instance.parameters();
    if (parameters.cut_thickness > 0
            && (bin_type.left_trim > 0
                || bin_type.right_trim > 0
                || bin_type.bottom_trim > 0
                || bin_type.top_trim > 0)) {
        return false;
    }
    return true;
}

namespace
{

/**
 * 'true' iff the nodes at depth 'depth' are delimited along 'x', i.e. are
 * the result of cuts along 'x' ('l' and 'r' are cut positions), for a
 * first cut orientation 'first_cut_orientation' ('Vertical' or
 * 'Horizontal'). Mirrors 'SolutionBuilder::add_node'.
 */
bool delimited_along_x(
        CutOrientation first_cut_orientation,
        Depth depth)
{
    return (first_cut_orientation == CutOrientation::Vertical && depth % 2 == 1)
        || (first_cut_orientation == CutOrientation::Horizontal && depth % 2 == 0);
}

}

bool Reduction::lift_item_dimensions_applies_axis(
        const Instance& instance,
        int axis)
{
    bool along_x = (axis == 0);
    if (along_x && instance.objective() == Objective::OpenDimensionX)
        return false;
    if (!along_x && instance.objective() == Objective::OpenDimensionY)
        return false;
    return items_can_shrink_along(instance, axis);
}

bool Reduction::items_can_shrink(const Instance& instance)
{
    const Parameters& parameters = instance.parameters();
    if (parameters.cut_type != CutType::NonExact
            && parameters.cut_type != CutType::Exact
            && parameters.cut_type != CutType::Roadef2018) {
        return false;
    }
    // Shrinking an item adds cuts, and the waste next to it may be small.
    if (parameters.minimum_distance_1_cuts > 0
            || parameters.minimum_distance_2_cuts > 0
            || parameters.minimum_waste_length > 0
            || parameters.maximum_number_1_cuts >= 0
            || parameters.maximum_number_2_cuts >= 0) {
        return false;
    }
    // A new cut could go through a defect.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        if (!instance.bin_type(bin_type_id).defects.empty())
            return false;
    }
    return true;
}

bool Reduction::items_can_shrink_along(
        const Instance& instance,
        int axis)
{
    // See the class-level doc comment for the cut type restrictions.
    bool along_x = (axis == 0);
    const Parameters& parameters = instance.parameters();
    if (parameters.cut_type == CutType::NonExact)
        return true;
    if (parameters.first_stage_orientation == CutOrientation::Any)
        return false;
    Depth depth = (parameters.cut_type == CutType::Exact)?
        parameters.number_of_stages:
        parameters.number_of_stages + 1;
    return delimited_along_x(parameters.first_stage_orientation, depth) == along_x;
}

bool Reduction::fits_maximum_distances(
        const Instance& instance,
        const Rectangle& rect)
{
    // 1-cuts are along 'x' if the first stage is vertical, 2-cuts along
    // the other axis.
    const Parameters& parameters = instance.parameters();
    CutOrientation first_stage_orientation = parameters.first_stage_orientation;
    if (parameters.maximum_distance_1_cuts >= 0) {
        if ((first_stage_orientation != CutOrientation::Horizontal
                    && rect.w > parameters.maximum_distance_1_cuts)
                || (first_stage_orientation != CutOrientation::Vertical
                    && rect.h > parameters.maximum_distance_1_cuts)) {
            return false;
        }
    }
    if (parameters.maximum_distance_2_cuts >= 0) {
        if ((first_stage_orientation != CutOrientation::Vertical
                    && rect.w > parameters.maximum_distance_2_cuts)
                || (first_stage_orientation != CutOrientation::Horizontal
                    && rect.h > parameters.maximum_distance_2_cuts)) {
            return false;
        }
    }
    return true;
}

bool Reduction::remove_dominated_items_applies(const Instance& instance)
{
    return instance.objective() == Objective::Knapsack
        && items_can_shrink(instance);
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

namespace
{

/** Largest value in 'schedule', or 0 if there is none. */
double maximum_consumption(const std::vector<double>* schedule)
{
    if (schedule == nullptr || schedule->empty())
        return 0.0;
    return *std::max_element(schedule->begin(), schedule->end());
}

/** Smallest value in 'schedule', or 0 if there is none. */
double minimum_consumption(const std::vector<double>* schedule)
{
    if (schedule == nullptr || schedule->empty())
        return 0.0;
    return *std::min_element(schedule->begin(), schedule->end());
}

/** The orientations of an item type: its dimensions, and rotated. */
std::vector<Rectangle> orientations(
        const Rectangle& rect,
        bool oriented)
{
    std::vector<Rectangle> rects = {rect};
    if (!oriented && rect.w != rect.h) {
        Rectangle rotated = rect;
        std::swap(rotated.w, rotated.h);
        rects.push_back(rotated);
    }
    return rects;
}

/** Width of 'bin_type' without its trims. */
Length usable_width(const BinType& bin_type)
{
    return bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
}

/** Height of 'bin_type' without its trims. */
Length usable_height(const BinType& bin_type)
{
    return bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
}

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
    const ReductionItemType& item_a = reduction_item_types[item_type_id_a];
    const ReductionItemType& item_b = reduction_item_types[item_type_id_b];
    if (item_type_a.profit < item_type_b.profit)
        return false;
    if (item_type_a.copies_fixed > 0 || item_type_b.copies_fixed > 0)
        return false;
    // Alone in their stacks (see 'items_mergeable').
    if (original_instance_->stack_size(item_type_a.stack_id) != item_type_a.copies
            || original_instance_->stack_size(item_type_b.stack_id) != item_type_b.copies) {
        return false;
    }

    // A fits inside B, whatever the orientation of B, without shrinking
    // along an axis items can't shrink along. The node of B can become a
    // plate.
    bool shrink_x = items_can_shrink_along(*original_instance_, 0);
    bool shrink_y = items_can_shrink_along(*original_instance_, 1);
    for (const Rectangle& rect_b: orientations(item_b.rect, item_b.oriented)) {
        if (!fits_maximum_distances(*original_instance_, rect_b))
            return false;
        bool fits = false;
        for (const Rectangle& rect_a: orientations(item_a.rect, item_a.oriented)) {
            if (rect_a.w > rect_b.w || rect_a.h > rect_b.h)
                continue;
            if (!shrink_x && rect_a.w != rect_b.w)
                continue;
            if (!shrink_y && rect_a.h != rect_b.h)
                continue;
            fits = true;
            break;
        }
        if (!fits)
            return false;
    }

    // Resources: the largest consumption of A must not exceed the smallest
    // consumption of B, since copies of A and B are not replaced at the
    // same copy index.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            if (maximum_consumption(find_item_type_schedule(item_type_a, bin_type_id, bin_type, resource_id))
                    > minimum_consumption(find_item_type_schedule(item_type_b, bin_type_id, bin_type, resource_id))) {
                return false;
            }
        }
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
    const ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
    const ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
    Length width = usable_width(bin_type);
    Length height = usable_height(bin_type);
    Length cut_thickness = original_instance_->parameters().cut_thickness;

    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (resource.penalize)
            continue;
        if (minimum_consumption(find_item_type_schedule(item_type_1, bin_type_id, bin_type, resource_id))
                + minimum_consumption(find_item_type_schedule(item_type_2, bin_type_id, bin_type, resource_id))
                > resource.capacity) {
            return true;
        }
    }

    // Two items fit together iff, in some orientations, a cut separates
    // them: side by side along 'x' or along 'y'.
    for (const Rectangle& rect_1: orientations(item_1.rect, item_1.oriented)) {
        if (rect_1.w > width || rect_1.h > height)
            continue;
        for (const Rectangle& rect_2: orientations(item_2.rect, item_2.oriented)) {
            if (rect_2.w > width || rect_2.h > height)
                continue;
            if (rect_1.w + cut_thickness + rect_2.w <= width
                    || rect_1.h + cut_thickness + rect_2.h <= height) {
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
    const ReductionItemType& item_a = reduction_item_types[item_type_id_a];
    const ReductionItemType& item_b = reduction_item_types[item_type_id_b];
    std::vector<DominatedItemsBinType> bin_types;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        Length width = usable_width(bin_type);
        Length height = usable_height(bin_type);
        auto fits = [width, height](const ReductionItemType& item)
        {
            for (const Rectangle& rect: orientations(item.rect, item.oriented))
                if (rect.w <= width && rect.h <= height)
                    return true;
            return false;
        };
        DominatedItemsBinType dominated_items_bin_type;
        dominated_items_bin_type.copies = bin_type.copies;
        dominated_items_bin_type.fits_b = fits(item_b);
        bool fits_a = fits(item_a);
        if (fits_a || dominated_items_bin_type.fits_b) {
            // A fits inside B, so it has the smallest area.
            Counter maximum_number_of_items = (width * height) / (item_a.rect.w * item_a.rect.h);
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
            item_a.copies,
            item_b.copies,
            bin_types);
}

void Reduction::remove_dominated_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
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
            if (item_b.copies == 0) {
                item_b.removed = true;
                break;
            }
        }
    }
}

bool Reduction::bin_type_dominates(
        BinTypeId bin_type_id_a,
        BinTypeId bin_type_id_b) const
{
    if (bin_type_id_a == bin_type_id_b)
        return false;
    const BinType& bin_type_a = original_instance_->bin_type(bin_type_id_a);
    const BinType& bin_type_b = original_instance_->bin_type(bin_type_id_b);
    const Parameters& parameters = original_instance_->parameters();

    if (bin_type_a.rect.w < bin_type_b.rect.w || bin_type_a.rect.h < bin_type_b.rect.h)
        return false;
    // The extra length of A becomes wastes and turns item nodes into
    // plates: see the class-level doc comment.
    if (parameters.minimum_distance_1_cuts > 0
            || parameters.minimum_distance_2_cuts > 0
            || parameters.maximum_distance_1_cuts >= 0
            || parameters.maximum_distance_2_cuts >= 0
            || parameters.minimum_waste_length > 0
            || parameters.maximum_number_1_cuts >= 0
            || parameters.maximum_number_2_cuts >= 0) {
        if (bin_type_a.rect.w != bin_type_b.rect.w || bin_type_a.rect.h != bin_type_b.rect.h)
            return false;
    }
    if (bin_type_a.left_trim != bin_type_b.left_trim
            || bin_type_a.right_trim != bin_type_b.right_trim
            || bin_type_a.bottom_trim != bin_type_b.bottom_trim
            || bin_type_a.top_trim != bin_type_b.top_trim
            || bin_type_a.left_trim_type != bin_type_b.left_trim_type
            || bin_type_a.right_trim_type != bin_type_b.right_trim_type
            || bin_type_a.bottom_trim_type != bin_type_b.bottom_trim_type
            || bin_type_a.top_trim_type != bin_type_b.top_trim_type) {
        return false;
    }
    if (bin_type_a.cost > bin_type_b.cost)
        return false;
    if (!bin_type_a.defects.empty() || !bin_type_b.defects.empty())
        return false;
    if (!bin_type_a.fixed_items.empty() || !bin_type_b.fixed_items.empty())
        return false;

    // Resources: a resource id carries no meaning across two bin types, so
    // any resource of A blocks dominance; B alone having resources is
    // fine, except a 'penalize' one with a negative 'penalty' (a profit
    // bonus A could never give) - see rectangle's own 'bin_type_dominates'.
    if (bin_type_a.number_of_resources() > 0)
        return false;
    for (ResourceId resource_id = 0;
            resource_id < bin_type_b.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type_b.resource(resource_id);
        if (resource.penalize && resource.penalty < 0)
            return false;
    }
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

Length Reduction::max_achievable_dimension_sum(
        const std::vector<ReductionItemType>& reduction_item_types,
        Length capacity,
        int axis,
        Length offset,
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
        subset_sum_item.lengths.push_back(((axis == 0)? item.rect.w: item.rect.h) + offset);
        if (!item.oriented)
            subset_sum_item.lengths.push_back(((axis == 0)? item.rect.h: item.rect.w) + offset);
        items.push_back(subset_sum_item);
    }
    return subset_sum_bound(items, capacity);
}

bool Reduction::lift_item_dimensions(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    const Parameters& parameters = original_instance_->parameters();
    const BinType& bin_type = original_instance_->bin_type(0);
    Length cut_thickness = parameters.cut_thickness;
    CutOrientation first_stage_orientation = parameters.first_stage_orientation;

    // For each maximum distance between cuts, the axes it may apply to.
    // 1-cuts are along 'x' if the first stage is vertical.
    struct MaximumDistance
    {
        Length distance;
        bool along_x;
        bool along_y;
    };
    std::vector<MaximumDistance> maximum_distances;
    if (parameters.maximum_distance_1_cuts >= 0) {
        maximum_distances.push_back({
                parameters.maximum_distance_1_cuts,
                first_stage_orientation != CutOrientation::Horizontal,
                first_stage_orientation != CutOrientation::Vertical});
    }
    if (parameters.maximum_distance_2_cuts >= 0) {
        maximum_distances.push_back({
                parameters.maximum_distance_2_cuts,
                first_stage_orientation != CutOrientation::Vertical,
                first_stage_orientation != CutOrientation::Horizontal});
    }

    bool found = false;
    for (int axis = 0; axis < 2; ++axis) {
        if (!lift_item_dimensions_applies_axis(*original_instance_, axis))
            continue;
        bool along_x = (axis == 0);

        // Lengths a chain of items along this axis must fit in: the bin
        // without its trims, and the maximum distances between cuts along
        // this axis. Each one is increased by the cut thickness and shrunk
        // to the largest total length of the items (each also increased by
        // the cut thickness) not exceeding it. Lifting an item type never
        // changes them: every combination of items containing the enlarged
        // item type still fits.
        std::vector<Length> capacities;
        capacities.push_back((along_x)?
                bin_type.rect.w - bin_type.left_trim - bin_type.right_trim:
                bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim);
        for (const MaximumDistance& maximum_distance: maximum_distances)
            if ((along_x)? maximum_distance.along_x: maximum_distance.along_y)
                capacities.push_back(maximum_distance.distance);
        std::vector<Length> shrunk_capacities;
        for (Length capacity: capacities) {
            shrunk_capacities.push_back(max_achievable_dimension_sum(
                        reduction_item_types,
                        capacity + cut_thickness,
                        axis,
                        cut_thickness,
                        -1));
        }

        for (ItemTypeId item_type_id = 0;
                item_type_id < (ItemTypeId)reduction_item_types.size();
                ++item_type_id) {
            ReductionItemType& item = reduction_item_types[item_type_id];
            if (item.removed)
                continue;
            // Oriented, so that growing one dimension only changes the
            // extent of the item along this axis, and a single copy, since
            // the bound below only holds for one occurrence of the item
            // type (see 'rectangle::Reduction::lift_item_dimensions_axis').
            if (!item.oriented || item.copies != 1)
                continue;
            const ItemType& item_type = original_instance_->item_type(item_type_id);
            if (item_type.copies_fixed > 0)
                continue;
            // Alone in its stack.
            if (original_instance_->stack_size(item_type.stack_id) != 1)
                continue;
            // 'unreduce_solution' may turn the node of the item into a
            // plate, subject to the maximum distances between cuts.
            if (!fits_maximum_distances(*original_instance_, item.rect))
                continue;

            Length& item_length = (along_x)? item.rect.w: item.rect.h;
            Length new_item_length = -1;
            for (Length shrunk_capacity: shrunk_capacities) {
                Length capacity = shrunk_capacity - (item_length + cut_thickness);
                Length achievable_other = (capacity <= 0)? 0:
                    max_achievable_dimension_sum(
                            reduction_item_types,
                            capacity,
                            axis,
                            cut_thickness,
                            item_type_id);
                Length length = shrunk_capacity - achievable_other - cut_thickness;
                if (new_item_length == -1 || new_item_length > length)
                    new_item_length = length;
            }
            if (new_item_length <= item_length)
                continue;
            item_length = new_item_length;
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

    const ReductionItemType& reduction_item_type_1 = reduction_item_types[item_type_id_1];
    const ReductionItemType& reduction_item_type_2 = reduction_item_types[item_type_id_2];
    if (reduction_item_type_1.rect.w != reduction_item_type_2.rect.w
            || reduction_item_type_1.rect.h != reduction_item_type_2.rect.h)
        return false;
    if (reduction_item_type_1.oriented != reduction_item_type_2.oriented)
        return false;

    // Profit is only compared for 'Knapsack' - see 'items_mergeable''s own
    // doc comment (and 'rectangle::Reduction::items_mergeable''s, for the
    // full argument).
    if (original_instance_->objective() == Objective::Knapsack
            && item_type_1.profit != item_type_2.profit)
        return false;

    // Both must be alone in their own stack - see 'items_mergeable''s own
    // doc comment for why a shared stack blocks the merge. "Alone" means no
    // *other* item type contributes to the same stack, not that the stack
    // holds exactly one physical item - see 'remove_negative_profit_items'
    // 's own comment on the identical check.
    if (original_instance_->stack_size(item_type_1.stack_id) != item_type_1.copies
            || original_instance_->stack_size(item_type_2.stack_id) != item_type_2.copies)
        return false;

    static const std::vector<double> empty_schedule;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const std::vector<double>* schedule_1_ptr
                = find_item_type_schedule(item_type_1, bin_type_id, bin_type, resource_id);
            const std::vector<double>* schedule_2_ptr
                = find_item_type_schedule(item_type_2, bin_type_id, bin_type, resource_id);
            const std::vector<double>& schedule_1
                = (schedule_1_ptr != nullptr)? *schedule_1_ptr: empty_schedule;
            const std::vector<double>& schedule_2
                = (schedule_2_ptr != nullptr)? *schedule_2_ptr: empty_schedule;
            if (schedule_1 != schedule_2)
                return false;
        }
    }

    // Each candidate's own current copies must be either entirely
    // mandatory or entirely optional, and both candidates must agree on
    // which - see 'rectangle::Reduction::items_mergeable''s own doc
    // comment for the concrete counterexample showing why a mixed merge
    // is unsound.
    ItemPos effective_copies_min_1 = effective_copies_min(reduction_item_types, item_type_id_1);
    ItemPos effective_copies_min_2 = effective_copies_min(reduction_item_types, item_type_id_2);
    const ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
    const ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
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

void Reduction::merge_identical_items(
        std::vector<ReductionItemType>& reduction_item_types)
{
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
        }
    }
}

Instance Reduction::reduction_to_instance(
        const std::vector<ReductionItemType>& reduction_item_types,
        const std::vector<bool>& bin_type_removed)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(original_instance_->objective());
    instance_builder.set_knapsack_goal(original_instance_->knapsack_goal());
    instance_builder.set_parameters(original_instance_->parameters());
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    // Every bin type not dominated (see 'compute_dominated_bin_types') is
    // copied over unchanged.
    reduced_to_original_bin_type_id_.clear();
    std::vector<BinTypeId> original_to_reduced_bin_type_id(
            original_instance_->number_of_bin_types(), -1);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        if (bin_type_id < (BinTypeId)bin_type_removed.size()
                && bin_type_removed[bin_type_id]) {
            continue;
        }
        original_to_reduced_bin_type_id[bin_type_id]
            = instance_builder.add_bin_type(*original_instance_, bin_type_id);
        reduced_to_original_bin_type_id_.push_back(bin_type_id);
    }

    // Renumber the stacks of the remaining item types consecutively. The
    // stacks of the removed item types are empty, and with their original
    // ids, the reduced instance would keep them: it would have more stacks
    // than item types, as if it had stacking constraints, even when every
    // item type is alone in its own stack.
    std::vector<StackId> stack_ids(original_instance_->number_of_stacks(), -1);
    StackId number_of_stacks = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        if (reduction_item_types[item_type_id].removed)
            continue;
        StackId stack_id = original_instance_->item_type(item_type_id).stack_id;
        if (stack_ids[stack_id] == -1) {
            stack_ids[stack_id] = number_of_stacks;
            number_of_stacks++;
        }
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

        const ItemType& original_item_type = original_instance_->item_type(item_type_id);
        ItemTypeId new_item_type_id = instance_builder.add_item_type(
                item.rect.w,
                item.rect.h,
                item.oriented,
                stack_ids[original_item_type.stack_id]);
        instance_builder.set_item_type_profit(new_item_type_id, original_item_type.profit);
        instance_builder.set_item_type_copies(new_item_type_id, total_copies);
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
        // Copy resource consumptions (the bin types themselves, including
        // their resources, were already copied above via 'add_bin_type(
        // *original_instance_, bin_type_id)', with their resources in the
        // same order).
        for (BinTypeId bin_type_id = 0;
                bin_type_id < original_instance_->number_of_bin_types();
                ++bin_type_id) {
            if (original_to_reduced_bin_type_id[bin_type_id] == -1)
                continue;
            const BinType& original_bin_type = original_instance_->bin_type(bin_type_id);
            for (const ItemResourceConsumption& consumption: original_item_type.resources[bin_type_id]) {
                const std::vector<double>& schedule
                    = original_bin_type.resource(consumption.resource_id)
                        .item_consumptions[consumption.consumption_pos].second;
                instance_builder.add_resource_consumption(
                        original_to_reduced_bin_type_id[bin_type_id],
                        consumption.resource_id,
                        new_item_type_id,
                        schedule);
            }
        }
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
    // Working representation: a stable 1:1 copy of the original instance's
    // item types (see 'ReductionItemType'). Always built and compacted
    // back via 'reduction_to_instance' at the end (even when
    // 'parameters.reduce' is 'false' below, in which case it is an
    // identity rebuild): this keeps 'reduced_item_origin_runs_' always
    // populated, so 'unreduce_solution' never needs a separate no-op code
    // path.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
        reduction_item_types[item_type_id].rect = instance.item_type(item_type_id).rect;
        reduction_item_types[item_type_id].oriented = instance.item_type(item_type_id).oriented;
    }

    if (parameters.reduce) {
        if (parameters.remove_redundant_rotations)
            remove_redundant_rotations(reduction_item_types);
        if (remove_negative_profit_items_applies(instance)
                && parameters.remove_negative_profit_items) {
            remove_negative_profit_items(reduction_item_types);
        }
        // Before lifting: the dominance compares the dimensions before it.
        if (parameters.remove_dominated_items
                && remove_dominated_items_applies(instance)) {
            remove_dominated_items(reduction_item_types);
        }
        if (parameters.lift_item_dimensions
                && lift_item_dimensions_applies(instance)
                && !parameters.timer.needs_to_end()) {
            lift_item_dimensions(reduction_item_types);
        }
        if (parameters.merge_identical_items)
            merge_identical_items(reduction_item_types);
    }

    for (const ReductionItemType& item: reduction_item_types) {
        reduced_rects_.push_back(item.rect);
        swapped_item_types_.push_back(item.swapped);
    }

    std::vector<bool> bin_type_removed;
    if (parameters.reduce
            && parameters.remove_dominated_bin_types
            && remove_dominated_bin_types_applies(instance)) {
        bin_type_removed = compute_dominated_bin_types();
    }

    instance_ = reduction_to_instance(reduction_item_types, bin_type_removed);
}

namespace
{

Length& node_start(
        SolutionNode& node,
        bool along_x)
{
    return (along_x)? node.l: node.b;
}

Length& node_end(
        SolutionNode& node,
        bool along_x)
{
    return (along_x)? node.r: node.t;
}

/** 'true' iff node 'node' is a waste without children. */
bool is_waste(const SolutionNode& node)
{
    return (node.item_type_id == -1 || node.item_type_id == -3)
        && node.children.empty();
}

/** Moves node 'node_id' and all its descendants by 'delta' along an axis. */
void shift_subtree(
        std::vector<SolutionNode>& nodes,
        SolutionNodeId node_id,
        bool along_x,
        Length delta)
{
    node_start(nodes[node_id], along_x) += delta;
    node_end(nodes[node_id], along_x) += delta;
    for (SolutionNodeId child_id: nodes[node_id].children)
        shift_subtree(nodes, child_id, along_x, delta);
}

/**
 * Shrinks node 'node_id', delimited along the axis, by 'delta' at its end.
 * Its next siblings are moved back by 'delta' up to the first waste, which
 * grows by 'delta' instead. Without a waste after the node, the freed length
 * becomes a waste at the end of the parent when the tree is replayed (see
 * 'SolutionBuilder::add_node'). This adds a child to the parent only if it
 * has no waste after the node.
 */
void shrink_in_parent(
        std::vector<SolutionNode>& nodes,
        SolutionNodeId node_id,
        bool along_x,
        Length delta)
{
    node_end(nodes[node_id], along_x) -= delta;
    Depth depth = nodes[node_id].d;
    bool after = false;
    for (SolutionNodeId sibling_id: nodes[nodes[node_id].f].children) {
        if (sibling_id == node_id) {
            after = true;
            continue;
        }
        if (!after || nodes[sibling_id].d != depth)
            continue;
        if (is_waste(nodes[sibling_id])) {
            node_start(nodes[sibling_id], along_x) -= delta;
            return;
        }
        shift_subtree(nodes, sibling_id, along_x, -delta);
    }
}

/**
 * Shrinks the item of leaf node 'node_id' by 'delta' along an axis (see
 * 'Reduction''s class-level doc comment): in place if the node is
 * delimited along the axis; otherwise, by moving the item into a child
 * node, if 'maximum_depth' allows it; otherwise, by shrinking the parent,
 * which must be delimited along the axis and only contain wastes besides
 * the item. Returns the id of the node containing the item.
 */
SolutionNodeId shrink_item_node(
        std::vector<SolutionNode>& nodes,
        SolutionNodeId node_id,
        bool along_x,
        Length delta,
        Depth maximum_depth,
        CutOrientation first_cut_orientation)
{
    Depth depth = nodes[node_id].d;
    if (delimited_along_x(first_cut_orientation, depth) == along_x) {
        shrink_in_parent(nodes, node_id, along_x, delta);
        return node_id;
    }

    if (depth + 1 <= maximum_depth) {
        SolutionNode child = nodes[node_id];
        child.f = node_id;
        child.d = depth + 1;
        child.children.clear();
        node_end(child, along_x) -= delta;
        SolutionNodeId child_id = nodes.size();
        nodes[node_id].item_type_id = -2;
        nodes[node_id].children = {child_id};
        nodes.push_back(child);
        return child_id;
    }

    SolutionNodeId parent_id = nodes[node_id].f;
    bool parent_ok = (nodes[parent_id].d >= 1);
    for (SolutionNodeId child_id: nodes[parent_id].children)
        if (child_id != node_id && !is_waste(nodes[child_id]))
            parent_ok = false;
    if (!parent_ok) {
        throw std::logic_error(
                FUNC_SIGNATURE + ": "
                "unable to shrink an enlarged item; "
                "depth: " + std::to_string(depth) + ".");
    }
    for (SolutionNodeId child_id: nodes[parent_id].children)
        node_end(nodes[child_id], along_x) -= delta;
    shrink_in_parent(nodes, parent_id, along_x, delta);
    return node_id;
}

/**
 * Adds the descendants of node 'node_id' of a cut tree to 'solution_builder',
 * in depth-first order (see 'SolutionBuilder::add_node': a node is appended
 * as the last child of the last node added at the parent depth). The nodes
 * of depth '0' or less (the root, the trims, and the plate inside the hard
 * trims) are not added, 'SolutionBuilder::add_bin' adds them, but their
 * descendants are.
 */
void add_subtree(
        SolutionBuilder& solution_builder,
        const std::vector<SolutionNode>& nodes,
        SolutionNodeId node_id,
        CutOrientation first_cut_orientation)
{
    for (SolutionNodeId child_id: nodes[node_id].children) {
        const SolutionNode& child = nodes[child_id];
        if (child.d <= 0) {
            add_subtree(solution_builder, nodes, child_id, first_cut_orientation);
            continue;
        }
        solution_builder.add_node(
                child.d,
                (delimited_along_x(first_cut_orientation, child.d))? child.r: child.t);
        if (child.item_type_id >= 0)
            solution_builder.set_last_node_item(child.item_type_id);
        add_subtree(solution_builder, nodes, child_id, first_cut_orientation);
    }
}

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

    // Deepest level of a node: the number of stages, plus the extra cut
    // for the cut types allowing one.
    const Parameters& parameters = original_instance_->parameters();
    Depth maximum_depth = (parameters.cut_type == CutType::Exact
            || parameters.cut_type == CutType::Homogenous)?
        parameters.number_of_stages:
        parameters.number_of_stages + 1;

    // A working copy of 'reduced_item_origin_runs_' that this call
    // consumes from (via 'consume_one_origin') as it scans 'solution' -
    // kept local rather than a member so nothing here needs resetting
    // between (or interferes with) separate 'unreduce_solution' calls.
    std::vector<std::vector<OriginRun>> remaining_runs = reduced_item_origin_runs_;

    // How many of the *current* pattern's leaf nodes reference each
    // reduced item type - needed to tell whether a whole physical bin's
    // worth of a given type can still be drawn from its current run
    // without crossing into the next one (see below). Declared once,
    // outside the bin loop, and only ever touched at the (few) item type
    // ids each pattern actually uses - 'touched_types' both drives that
    // targeted reset and stands in for the distinct-types set itself, so
    // nothing here needs a 'std::map'.
    std::vector<ItemPos> slot_count(instance_.number_of_item_types(), 0);
    std::vector<ItemTypeId> touched_types;

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);

        touched_types.clear();
        for (const SolutionNode& node: solution_bin.nodes) {
            // The root node (depth 0) reuses 'item_type_id' to store the
            // *bin* type id instead (see 'SolutionBuilder::add_bin') -
            // 'd <= 0' also excludes trim nodes, which already carry
            // '-1' there, but the root's own 'bin_type_id' can otherwise
            // easily alias a real item type id and corrupt the counts
            // below. Mirrors the exact same 'd <= 0' guard the real
            // replay loop already uses further down.
            if (node.d <= 0 || node.item_type_id < 0)
                continue;
            if (slot_count[node.item_type_id] == 0)
                touched_types.push_back(node.item_type_id);
            slot_count[node.item_type_id]++;
        }

        BinPos remaining_bin_copies = solution_bin.copies;
        while (remaining_bin_copies > 0) {
            // 'true' iff every reduced item type this pattern places can
            // supply this whole bin's worth of copies from its *current*
            // run alone - i.e. resolving one physical bin's worth of
            // placements is guaranteed not to cross any type's run
            // boundary partway through. Whenever this holds, an entire
            // batch of consecutive physical bins - not just this one -
            // can share a single 'add_bin' entry (see below); the common
            // case (a type that was never merged at all has only one run
            // ever, so this is always 'true' for it) is what keeps this
            // whole loop down to a handful of iterations, regardless of
            // how many physical copies there actually are.
            bool bin_fits_in_current_runs = true;
            for (ItemTypeId reduced_item_type_id: touched_types) {
                if (remaining_runs[reduced_item_type_id].back().count
                        < slot_count[reduced_item_type_id]) {
                    bin_fits_in_current_runs = false;
                    break;
                }
            }

            // Every slot's origin is fixed for as long as this batch
            // lasts (each type stays on its current run throughout) when
            // 'bin_fits_in_current_runs' holds - the largest such batch
            // is the tightest of every placed type's own 'floor(remaining
            // in its current run / copies needed per bin)'. Otherwise, at
            // least one placed type would run out mid-bin, so this bin is
            // resolved the slow way instead, one physical copy at a time
            // (still only ever as many 'consume_one_origin' calls as this
            // one bin has item slots, not proportional to
            // 'remaining_bin_copies').
            BinPos batch = 1;
            if (bin_fits_in_current_runs) {
                batch = remaining_bin_copies;
                for (ItemTypeId reduced_item_type_id: touched_types) {
                    batch = std::min(
                            batch,
                            (BinPos)(remaining_runs[reduced_item_type_id].back().count
                                / slot_count[reduced_item_type_id]));
                }
            }

            // Copy of the cut tree, with the original item type id of each
            // item. Resolved in the stored order of the nodes, any
            // consistent order works since a reduced item type's copies
            // are interchangeable by construction.
            // The children are rebuilt from the parents, in the stored
            // order of the nodes, which is the order they were built in.
            std::vector<SolutionNode> nodes = solution_bin.nodes;
            for (SolutionNode& node: nodes)
                node.children.clear();
            for (SolutionNodeId node_id = 0;
                    node_id < (SolutionNodeId)nodes.size();
                    ++node_id) {
                SolutionNode& node = nodes[node_id];
                if (node.f >= 0)
                    nodes[node.f].children.push_back(node_id);
                if (node.d <= 0 || node.item_type_id < 0)
                    continue;
                node.item_type_id = (bin_fits_in_current_runs)?
                    remaining_runs[node.item_type_id].back().item_type_id:
                    consume_one_origin(remaining_runs[node.item_type_id]);
            }

            // Give each item enlarged by 'lift_item_dimensions' its true
            // dimensions (see the class-level doc comment).
            SolutionNodeId number_of_nodes = nodes.size();
            for (SolutionNodeId node_id = 0; node_id < number_of_nodes; ++node_id) {
                if (nodes[node_id].d <= 0 || nodes[node_id].item_type_id < 0)
                    continue;
                ItemTypeId item_type_id = nodes[node_id].item_type_id;
                // True dimensions, in the orientation of the reduced item
                // type, then in the orientation of the node.
                Rectangle rect = original_instance_->item_type(item_type_id).rect;
                if (swapped_item_types_[item_type_id])
                    std::swap(rect.w, rect.h);
                const Rectangle& reduced_rect = reduced_rects_[item_type_id];
                const SolutionNode& node = nodes[node_id];
                if (node.r - node.l != reduced_rect.w
                        || node.t - node.b != reduced_rect.h) {
                    std::swap(rect.w, rect.h);
                }
                Length delta_x = (node.r - node.l) - rect.w;
                Length delta_y = (node.t - node.b) - rect.h;
                if (delta_x < 0 || delta_y < 0) {
                    throw std::logic_error(
                            FUNC_SIGNATURE + ": "
                            "item larger than its node; "
                            "item_type_id: " + std::to_string(item_type_id) + ".");
                }
                // First along the axis the node is delimited along, then
                // along the other one (see 'shrink_item_node').
                bool first_along_x = delimited_along_x(solution_bin.first_cut_orientation, node.d);
                Length first_delta = (first_along_x)? delta_x: delta_y;
                Length second_delta = (first_along_x)? delta_y: delta_x;
                SolutionNodeId item_node_id = node_id;
                if (first_delta > 0) {
                    item_node_id = shrink_item_node(
                            nodes,
                            item_node_id,
                            first_along_x,
                            first_delta,
                            maximum_depth,
                            solution_bin.first_cut_orientation);
                }
                if (second_delta > 0) {
                    shrink_item_node(
                            nodes,
                            item_node_id,
                            !first_along_x,
                            second_delta,
                            maximum_depth,
                            solution_bin.first_cut_orientation);
                }
            }

            // Replay the cut tree (see 'add_subtree').
            solution_builder.add_bin(
                    reduced_to_original_bin_type_id_[solution_bin.bin_type_id],
                    batch,
                    solution_bin.first_cut_orientation);
            add_subtree(
                    solution_builder,
                    nodes,
                    0,
                    solution_bin.first_cut_orientation);

            if (bin_fits_in_current_runs) {
                for (ItemTypeId reduced_item_type_id: touched_types) {
                    OriginRun& run = remaining_runs[reduced_item_type_id].back();
                    run.count -= slot_count[reduced_item_type_id] * batch;
                    if (run.count == 0)
                        remaining_runs[reduced_item_type_id].pop_back();
                }
            }
            remaining_bin_copies -= batch;
        }

        for (ItemTypeId reduced_item_type_id: touched_types)
            slot_count[reduced_item_type_id] = 0;
    }

    return solution_builder.build();
}
