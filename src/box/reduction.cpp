#include "packingsolver/box/reduction.hpp"

#include "packingsolver/box/instance_builder.hpp"
#include "box/solution_builder.hpp"
#include "algorithms/subset_sum_bound.hpp"
#include "algorithms/dominated_items.hpp"

#include <algorithm>
#include <array>

using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

/**
 * The consumption schedule 'item_type' has for 'resource_id' in bin type
 * 'bin_type_id' of 'bin_type' (see 'ItemType::resources'), or 'nullptr' if
 * it has none there. A linear scan over 'item_type.resources[bin_type_id]'
 * - typically tiny, how many resources a single item type is involved in -
 * not a search through the resource's own (potentially larger) item list.
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

/**
 * The set of rotations 'rotations' allows, as a fixed-size boolean array
 * indexed by 'Rotation' - lets two item types' allowed-rotation sets be
 * compared regardless of the order 'rotations' happens to list them in.
 */
std::array<bool, NUMBER_OF_ROTATIONS> rotation_set(const std::vector<Rotation>& rotations)
{
    std::array<bool, NUMBER_OF_ROTATIONS> set{};
    for (Rotation rotation: rotations)
        set[(int)rotation] = true;
    return set;
}

/** Length of 'box' along axis 'axis' ('0' for 'x', '1' for 'y', '2' for 'z'). */
Length box_length(
        const Box& box,
        int axis)
{
    return (axis == 0)? box.x: (axis == 1)? box.y: box.z;
}

/** Mutable version of 'box_length'. */
Length& box_length(
        Box& box,
        int axis)
{
    return (axis == 0)? box.x: (axis == 1)? box.y: box.z;
}

/**
 * The dimension of an item (in its own, unrotated frame) that rotation
 * 'rotation' puts along axis 'axis'.
 */
int rotated_dimension(
        Rotation rotation,
        int axis)
{
    Box box = {0, 1, 2};
    return (int)box_length(box.rotate(rotation), axis);
}

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

    // Lifting can make more rotations redundant (two rotations giving the
    // same enlarged box, or a rotation of the enlarged box fitting in no
    // bin type), and removing rotations can enable lifting (it requires
    // every allowed rotation to put the same dimension along the axis) and
    // perfect pairs (they require a single allowed rotation). Lifting can
    // make an item fill the shrunk bin, and a dedicated bin can shrink the
    // bin further.
    bool run_lift = parameters.lift_item_dimensions
        && lift_item_dimensions_applies(instance);
    bool full_bin_reduction = full_bin_reduction_applies(instance);
    bool run_full_bin_items = full_bin_reduction && parameters.reduce_full_bin_items;
    bool run_perfect_pairs = full_bin_reduction && parameters.reduce_perfect_pairs;
    while (run_lift || run_full_bin_items || run_perfect_pairs) {
        if (parameters.timer.needs_to_end())
            break;
        bool found = false;
        if (run_lift)
            found |= lift_item_dimensions(reduction_item_types);
        if (found && parameters.remove_redundant_rotations)
            remove_redundant_rotations(reduction_item_types);
        if (run_full_bin_items)
            found |= reduce_full_bin_items(reduction_item_types);
        if (run_perfect_pairs)
            found |= reduce_perfect_pairs(reduction_item_types);
        if (!found)
            break;
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

bool Reduction::remove_negative_profit_items_applies(const Instance& instance)
{
    if (instance.objective() != Objective::Knapsack)
        return false;

    // A 'penalize' resource with a negative 'penalty' is a one-time profit
    // bonus the first time a bin's consumption crosses its capacity - a
    // negative-profit item could still be worth including if its own
    // consumption helps trigger that crossing, an indirect benefit this
    // purely per-item check cannot see.
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
    // For 'Knapsack', 'reduction_item_types[item_type_id].copies' can only
    // ever have shrunk below 'original_item_type.copies' via
    // 'remove_negative_profit_items' - copies trimmed away that way are
    // simply gone, never placed anywhere - so the requirement stays
    // exactly 'original_item_type.copies_min', just capped by whatever
    // copies genuinely remain (never below current 'copies', which would
    // be unsatisfiable). 'remove_negative_profit_items' only ever trims
    // down to exactly 'copies_min' itself, so this capping is never
    // actually tightened in practice - it only guards the invariant.
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
        if (item.removed)
            continue;
        const ItemType& item_type = original_instance_->item_type(item_type_id);
        if (item_type.copies_fixed > 0)
            continue;

        // Keep the first rotation of each rotated box.
        std::vector<Rotation> distinct_rotations;
        for (Rotation rotation: item.rotations) {
            Box box = item.box.rotate(rotation);
            bool duplicate = false;
            for (Rotation distinct_rotation: distinct_rotations) {
                if (item.box.rotate(distinct_rotation) == box) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                distinct_rotations.push_back(rotation);
        }

        // Remove the rotations whose rotated box fits in no bin type, unless
        // none fits.
        std::vector<Rotation> fitting_rotations;
        for (Rotation rotation: distinct_rotations) {
            Box box = item.box.rotate(rotation);
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < original_instance_->number_of_bin_types();
                    ++bin_type_id) {
                const Box& bin_box = original_instance_->bin_type(bin_type_id).box;
                if (box.x <= bin_box.x
                        && box.y <= bin_box.y
                        && box.z <= bin_box.z) {
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

bool Reduction::remove_dominated_items_applies(const Instance& instance)
{
    return instance.objective() == Objective::Knapsack;
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

/** 'true' iff 'box_1' fits inside 'box_2'. */
bool fits_inside(
        const Box& box_1,
        const Box& box_2)
{
    return box_1.x <= box_2.x
        && box_1.y <= box_2.y
        && box_1.z <= box_2.z;
}

/** 'true' iff 'box' fits in 'bin_box' with one of the rotations 'rotations'. */
bool fits_in_bin(
        const Box& box,
        const std::vector<Rotation>& rotations,
        const Box& bin_box)
{
    for (Rotation rotation: rotations)
        if (fits_inside(box.rotate(rotation), bin_box))
            return true;
    return false;
}

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
    if (item_type_a.weight > item_type_b.weight)
        return false;
    if (item_type_a.copies_fixed > 0 || item_type_b.copies_fixed > 0)
        return false;

    // A fits inside B, whatever the rotation of B.
    for (Rotation rotation_b: item_b.rotations) {
        Box box_b = item_type_b.box.rotate(rotation_b);
        bool fits = false;
        for (Rotation rotation_a: item_a.rotations) {
            if (fits_inside(item_type_a.box.rotate(rotation_a), box_b)) {
                fits = true;
                break;
            }
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

    if (item_type_1.weight + item_type_2.weight > bin_type.maximum_weight)
        return true;
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

    // Two boxes fit together iff, with some rotations, they fit side by
    // side along some axis.
    for (Rotation rotation_1: reduction_item_types[item_type_id_1].rotations) {
        Box box_1 = item_type_1.box.rotate(rotation_1);
        if (!fits_inside(box_1, bin_type.box))
            continue;
        for (Rotation rotation_2: reduction_item_types[item_type_id_2].rotations) {
            Box box_2 = item_type_2.box.rotate(rotation_2);
            if (!fits_inside(box_2, bin_type.box))
                continue;
            if (box_1.x + box_2.x <= bin_type.box.x
                    || box_1.y + box_2.y <= bin_type.box.y
                    || box_1.z + box_2.z <= bin_type.box.z) {
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
    std::vector<DominatedItemsBinType> bin_types;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = original_instance_->bin_type(bin_type_id);
        DominatedItemsBinType dominated_items_bin_type;
        dominated_items_bin_type.copies = bin_type.copies;
        dominated_items_bin_type.fits_b = fits_in_bin(
                item_type_b.box,
                reduction_item_types[item_type_id_b].rotations,
                bin_type.box);
        bool fits_a = fits_in_bin(
                item_type_a.box,
                reduction_item_types[item_type_id_a].rotations,
                bin_type.box);
        if (fits_a || dominated_items_bin_type.fits_b) {
            // A fits inside B, so it has the smallest volume.
            Counter maximum_number_of_items = bin_type.box.volume() / item_type_a.box.volume();
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
    if (!fits_inside(bin_type_b.box, bin_type_a.box))
        return false;
    if (bin_type_a.cost > bin_type_b.cost)
        return false;
    if (bin_type_a.maximum_weight < bin_type_b.maximum_weight)
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

bool Reduction::full_bin_reduction_applies(const Instance& instance)
{
    if (instance.objective() != Objective::BinPacking)
        return false;
    // A single bin type, so that the bin of the reservations is
    // unambiguous, without fixed items (they would share the dedicated
    // bins).
    if (instance.number_of_bin_types() != 1)
        return false;
    const BinType& bin_type = instance.bin_type(0);
    if (!bin_type.fixed_items.empty())
        return false;
    // The exchange arguments move items between bins: a weight or resource
    // constraint, a whole-bin aggregate, could be violated.
    if (instance.resources_matter())
        return false;
    if (bin_type.maximum_weight != std::numeric_limits<Weight>::infinity()) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (instance.item_type(item_type_id).weight != 0)
                return false;
        }
    }
    return true;
}

BinPos Reduction::number_of_dedicated_bins() const
{
    BinPos total = 0;
    for (const FullBinItem& full_bin_item: full_bin_items_)
        total += full_bin_item.copies;
    for (const PerfectPair& pair: perfect_pairs_)
        total += pair.copies;
    return total;
}

bool Reduction::reduce_full_bin_items(
        std::vector<ReductionItemType>& reduction_item_types)
{
    const BinType& bin_type = original_instance_->bin_type(0);
    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed || item.copies == 0)
            continue;
        if (original_instance_->item_type(item_type_id).copies_fixed > 0)
            continue;
        // Recomputed for each item type, since each reservation removes
        // items and can shrink the bin.
        Box shrunk_bin = {
            shrunk_bin_length(reduction_item_types, 0),
            shrunk_bin_length(reduction_item_types, 1),
            shrunk_bin_length(reduction_item_types, 2)};
        bool matches = false;
        Rotation rotation = Rotation::XYZ;
        for (Rotation r: item.rotations) {
            if (item.box.rotate(r) == shrunk_bin) {
                matches = true;
                rotation = r;
                break;
            }
        }
        if (!matches)
            continue;
        // Never more dedicated bins than the bin type has copies.
        if (number_of_dedicated_bins() + item.copies > bin_type.copies)
            continue;
        full_bin_items_.push_back({item_type_id, rotation, item.copies});
        item.copies = 0;
        item.removed = true;
        found = true;
    }
    return found;
}

bool Reduction::reduce_perfect_pairs(
        std::vector<ReductionItemType>& reduction_item_types)
{
    const BinType& bin_type = original_instance_->bin_type(0);
    bool found = false;
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_1) {
        ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
        if (item_1.removed || item_1.copies == 0 || item_1.rotations.size() != 1)
            continue;
        if (original_instance_->item_type(item_type_id_1).copies_fixed > 0)
            continue;
        for (ItemTypeId item_type_id_2 = item_type_id_1;
                item_type_id_2 < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_2) {
            ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
            if (item_2.removed || item_2.copies == 0 || item_2.rotations.size() != 1)
                continue;
            if (original_instance_->item_type(item_type_id_2).copies_fixed > 0)
                continue;
            // Two copies of the same item type.
            if (item_type_id_2 == item_type_id_1 && item_1.copies < 2)
                continue;

            Box shrunk_bin = {
                shrunk_bin_length(reduction_item_types, 0),
                shrunk_bin_length(reduction_item_types, 1),
                shrunk_bin_length(reduction_item_types, 2)};
            Box box_1 = item_1.box.rotate(item_1.rotations.front());
            Box box_2 = item_2.box.rotate(item_2.rotations.front());
            int pair_axis = -1;
            for (int axis = 0; axis < 3; ++axis) {
                bool other_axes_full = true;
                for (int other_axis = 0; other_axis < 3; ++other_axis) {
                    if (other_axis == axis)
                        continue;
                    if (box_length(box_1, other_axis) != box_length(shrunk_bin, other_axis)
                            || box_length(box_2, other_axis) != box_length(shrunk_bin, other_axis)) {
                        other_axes_full = false;
                    }
                }
                if (other_axes_full
                        && box_length(box_1, axis) + box_length(box_2, axis)
                        == box_length(shrunk_bin, axis)) {
                    pair_axis = axis;
                    break;
                }
            }
            if (pair_axis == -1)
                continue;

            ItemPos copies = (item_type_id_2 == item_type_id_1)?
                item_1.copies / 2:
                (std::min)(item_1.copies, item_2.copies);
            // Never more dedicated bins than the bin type has copies.
            if (number_of_dedicated_bins() + copies > bin_type.copies)
                continue;
            perfect_pairs_.push_back({
                    item_type_id_1,
                    item_type_id_2,
                    item_1.rotations.front(),
                    item_2.rotations.front(),
                    pair_axis,
                    copies});
            if (item_type_id_2 == item_type_id_1) {
                item_1.copies -= 2 * copies;
            } else {
                item_1.copies -= copies;
                item_2.copies -= copies;
                if (item_2.copies == 0)
                    item_2.removed = true;
            }
            if (item_1.copies == 0)
                item_1.removed = true;
            found = true;
            if (item_1.removed)
                break;
        }
    }
    return found;
}

bool Reduction::lift_item_dimensions_applies(const Instance& instance)
{
    // A single bin type, so that the bin dimensions are unambiguous, and
    // no fixed items, which the compaction argument of the class-level doc
    // comment cannot move.
    return instance.number_of_bin_types() == 1
        && instance.bin_type(0).fixed_items.empty();
}

bool Reduction::lift_item_dimensions_applies_axis(
        const Instance& instance,
        int axis)
{
    // See the class-level doc comment: the downstream solve would reason
    // against the enlarged items instead of the true ones.
    Objective open_dimension_objective
        = (axis == 0)? Objective::OpenDimensionX:
        (axis == 1)? Objective::OpenDimensionY:
        Objective::OpenDimensionZ;
    if (instance.objective() == open_dimension_objective)
        return false;
    if (instance.objective() == Objective::BinPackingWithLeftovers) {
        LeftoverMode leftover_mode = instance.parameters().leftover_mode;
        bool x = (leftover_mode == LeftoverMode::X
                || leftover_mode == LeftoverMode::XY
                || leftover_mode == LeftoverMode::XZ
                || leftover_mode == LeftoverMode::XYZ);
        bool y = (leftover_mode == LeftoverMode::Y
                || leftover_mode == LeftoverMode::XY
                || leftover_mode == LeftoverMode::YZ
                || leftover_mode == LeftoverMode::XYZ);
        bool z = (leftover_mode == LeftoverMode::Z
                || leftover_mode == LeftoverMode::XZ
                || leftover_mode == LeftoverMode::YZ
                || leftover_mode == LeftoverMode::XYZ);
        if ((axis == 0 && x) || (axis == 1 && y) || (axis == 2 && z))
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
        for (Rotation rotation: item.rotations)
            subset_sum_item.lengths.push_back(box_length(item.box.rotate(rotation), axis));
        items.push_back(subset_sum_item);
    }
    return subset_sum_bound(items, capacity);
}

Length Reduction::shrunk_bin_length(
        const std::vector<ReductionItemType>& reduction_item_types,
        int axis) const
{
    return max_achievable_dimension_sum(
            reduction_item_types,
            box_length(original_instance_->bin_type(0).box, axis),
            axis,
            -1);
}

bool Reduction::lift_item_dimensions(
        std::vector<ReductionItemType>& reduction_item_types) const
{
    bool found = false;
    for (int axis = 0; axis < 3; ++axis) {
        if (!lift_item_dimensions_applies_axis(*original_instance_, axis))
            continue;

        // Lifting an item type never changes the shrunk bin: every
        // combination of items containing the enlarged item type still
        // fits in it.
        Length bin_length = shrunk_bin_length(reduction_item_types, axis);

        for (ItemTypeId item_type_id = 0;
                item_type_id < (ItemTypeId)reduction_item_types.size();
                ++item_type_id) {
            ReductionItemType& item = reduction_item_types[item_type_id];
            if (item.removed || item.rotations.empty())
                continue;
            // A single copy: the bound below only holds for one occurrence
            // of the item type - every copy of an enlarged item type is
            // enlarged, while the other items could fill the gap beside
            // each of them separately (see
            // 'rectangle::Reduction::lift_item_dimensions_axis').
            if (item.copies != 1)
                continue;
            if (original_instance_->item_type(item_type_id).copies_fixed > 0)
                continue;
            // Every allowed rotation must put the same dimension of the
            // item along this axis, so that growing it only changes the
            // extent of the item along this axis.
            int dimension = rotated_dimension(item.rotations.front(), axis);
            bool same_dimension = true;
            for (Rotation rotation: item.rotations) {
                if (rotated_dimension(rotation, axis) != dimension) {
                    same_dimension = false;
                    break;
                }
            }
            if (!same_dimension)
                continue;

            Length& item_length = box_length(item.box, dimension);
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

    // Current boxes: two item types with the same original box may have
    // been enlarged differently by 'lift_item_dimensions'.
    if (reduction_item_types[item_type_id_1].box != reduction_item_types[item_type_id_2].box)
        return false;
    if (rotation_set(reduction_item_types[item_type_id_1].rotations)
            != rotation_set(reduction_item_types[item_type_id_2].rotations))
        return false;
    if (item_type_1.weight != item_type_2.weight)
        return false;

    // Profit is only compared for 'Knapsack': for every other objective
    // this class handles, profit is never the actual objective, and
    // 'unreduce_solution' always restores the true original item type id
    // (and so its true profit) for every placed copy regardless of which
    // one a reduced-instance solve actually used. For 'Knapsack', profit
    // *is* the actual objective, and copies are optional, so merging two
    // different-profit item types would report a single, uniform profit
    // for every copy in the reduced instance and let the solve itself
    // optimize against wrong values.
    if (original_instance_->objective() == Objective::Knapsack
            && item_type_1.profit != item_type_2.profit)
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
            // The consumption of a copy depends on the number of copies of
            // its item type already in the bin: after the merge, the copies
            // of the second item type would be counted as further copies of
            // the first one, so the schedule must be the same for every copy.
            for (double consumption: schedule_1)
                if (consumption != schedule_1.front())
                    return false;
        }
    }

    // Each candidate's own current copies must be either entirely
    // mandatory or entirely optional, and both candidates must agree on
    // which - a mix would let a downstream 'Knapsack' solve satisfy the
    // merged survivor's combined 'copies_min' entirely out of the
    // originally-optional type's own share, leaving the originally-
    // mandatory type's copies unplaced.
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
    instance_builder.set_original_instance(*original_instance_);
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    // 'add_item_type_from_original_instance' below only copies the
    // resource consumptions of the bin types added here.
    reduced_to_original_bin_type_id_.clear();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        if (bin_type_id < (BinTypeId)bin_type_removed.size()
                && bin_type_removed[bin_type_id]) {
            continue;
        }
        BinTypeId new_bin_type_id = instance_builder.add_bin_type_from_original_instance(bin_type_id);
        reduced_to_original_bin_type_id_.push_back(bin_type_id);
        // The dedicated bins (see 'reduce_full_bin_items'/
        // 'reduce_perfect_pairs', single bin type) are left out of the
        // reduced instance. They never take more than the bin type's
        // copies; if they take all of them while items remain, the
        // instance is infeasible.
        BinPos number_of_dedicated_bins = this->number_of_dedicated_bins();
        if (number_of_dedicated_bins > 0) {
            BinPos copies = original_instance_->bin_type(bin_type_id).copies - number_of_dedicated_bins;
            if (copies > 0) {
                instance_builder.set_bin_type_copies(new_bin_type_id, copies);
            } else {
                bool remaining_items = false;
                for (const ReductionItemType& item: reduction_item_types)
                    if (item.copies > 0)
                        remaining_items = true;
                // Without remaining items, the bin type keeps its copies,
                // never used.
                if (remaining_items)
                    proven_infeasible_ = true;
            }
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

        // 'add_item_type_from_original_instance' copies every field
        // (box, rotations, profit, weight, resource consumptions) from the
        // survivor's own original item type; only 'copies'/'copies_min'
        // need overriding afterwards to reflect the merge, 'rotations' to
        // reflect 'remove_redundant_rotations', and 'box' to reflect
        // 'lift_item_dimensions'.
        ItemTypeId new_item_type_id = instance_builder.add_item_type_from_original_instance(item_type_id);
        instance_builder.set_item_type_copies(new_item_type_id, total_copies);
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
        instance_builder.set_item_type_rotations(new_item_type_id, item.rotations);
        instance_builder.set_item_type_box(new_item_type_id, item.box);

        reduced_item_origin_runs_.push_back(std::move(origin_runs));
    }

    return instance_builder.build();
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

    // How many of the *current* pattern's item slots reference each
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
        for (const SolutionItem& solution_item: solution_bin.items) {
            if (slot_count[solution_item.item_type_id] == 0)
                touched_types.push_back(solution_item.item_type_id);
            slot_count[solution_item.item_type_id]++;
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
                for (const SolutionItem& solution_item: solution_bin.items) {
                    ItemTypeId original_item_type_id
                        = remaining_runs[solution_item.item_type_id].back().item_type_id;
                    solution_builder.add_item(
                            new_bin_pos,
                            original_item_type_id,
                            solution_item.bl_corner,
                            solution_item.rotation);
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
                for (const SolutionItem& solution_item: solution_bin.items) {
                    ItemTypeId original_item_type_id
                        = consume_one_origin(remaining_runs[solution_item.item_type_id]);
                    solution_builder.add_item(
                            new_bin_pos,
                            original_item_type_id,
                            solution_item.bl_corner,
                            solution_item.rotation);
                }
                remaining_bin_copies--;
            }
        }

        for (ItemTypeId reduced_item_type_id: touched_types)
            slot_count[reduced_item_type_id] = 0;
    }

    // The dedicated bins, left out of 'instance_' (single bin type, see
    // 'full_bin_reduction_applies'). A pair is placed side by side along
    // its axis, with the true boxes of the items.
    for (const FullBinItem& full_bin_item: full_bin_items_) {
        BinPos new_bin_pos = solution_builder.add_bin(0, full_bin_item.copies);
        solution_builder.add_item(
                new_bin_pos,
                full_bin_item.item_type_id,
                {0, 0, 0},
                full_bin_item.rotation);
    }
    for (const PerfectPair& pair: perfect_pairs_) {
        BinPos new_bin_pos = solution_builder.add_bin(0, pair.copies);
        solution_builder.add_item(
                new_bin_pos,
                pair.item_type_id_1,
                {0, 0, 0},
                pair.rotation_1);
        Box box_1 = original_instance_->item_type(pair.item_type_id_1).box.rotate(pair.rotation_1);
        Point position = {0, 0, 0};
        if (pair.axis == 0) {
            position.x = box_1.x;
        } else if (pair.axis == 1) {
            position.y = box_1.y;
        } else {
            position.z = box_1.z;
        }
        solution_builder.add_item(
                new_bin_pos,
                pair.item_type_id_2,
                position,
                pair.rotation_2);
    }

    return solution_builder.build();
}
