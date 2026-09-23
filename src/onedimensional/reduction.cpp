#include "packingsolver/onedimensional/reduction.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/solution_builder.hpp"

#include "subsetsumsolver/instance_builder.hpp"
#include "subsetsumsolver/algorithms/dynamic_programming_bellman.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

/**
 * 'item_type''s own resource consumption schedule for
 * '(bin_type_id, resource_id)', or 'nullptr' if it has none (implicitly a
 * zero-consumption schedule) - same helper (same name, same semantics) as
 * 'rectangle::reduction.cpp''s own, reimplemented locally here since it is
 * not shared/exported.
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

/** 'true' iff 'item_type' is involved, as either side, in a precedence. */
bool has_precedence(const ItemType& item_type)
{
    return !item_type.dominated_precedence_ids.empty()
        || !item_type.dominating_precedence_ids.empty();
}

}

bool Reduction::remove_negative_profit_items_applies(const Instance& instance)
{
    if (instance.objective() != Objective::Knapsack)
        return false;
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

bool Reduction::is_classical_knapsack(const Instance& instance)
{
    // A single bin: 'optimize()' then runs
    // 'knapsacksolver::dynamic_programming_primal_dual' on it...
    if (instance.objective() != Objective::Knapsack
            || instance.number_of_bins() != 1) {
        return false;
    }
    // ... which is only exact when none of the constraints it relaxes or
    // ignores is present (otherwise it still runs, for its bound and a
    // quick feasible solution, but other algorithms may still benefit from
    // the reduction).
    const BinType& bin_type = instance.bin_type(0);
    if (!bin_type.fixed_items.empty()
            || instance.weight_matters()
            || instance.resources_matter()
            || !instance.precedences().empty()) {
        return false;
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.nesting_length != 0
                || item_type.maximum_weight_after != std::numeric_limits<Weight>::infinity()
                || item_type.maximum_stackability < instance.number_of_items()
                || item_type.copies_min != 0
                || !instance.item_type_fits_bin_type(item_type_id, 0)) {
            return false;
        }
    }
    return true;
}

bool Reduction::full_bin_reduction_applies(const Instance& instance)
{
    // Only 'BinPacking' - see this class's own doc comment. (Unlike
    // rectangle's own 'companion_absorption_applies', not also
    // 'VariableSizedBinPacking'/'Feasibility': out of scope for now.)
    if (instance.objective() != Objective::BinPacking
            || instance.number_of_bin_types() != 1
            || !instance.bin_type(0).fixed_items.empty()
            || instance.weight_matters()
            || instance.resources_matter()) {
        return false;
    }
    // Every dedicated-bin reservation relies on an exchange argument that
    // moves items between bins of an optimal solution, so every item type
    // of the instance - not only the reserved ones - must take exactly its
    // own length wherever it goes, with no constraint on what shares its
    // bin.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.nesting_length != 0)
            return false;
        if (item_type.maximum_weight_after != std::numeric_limits<Weight>::infinity())
            return false;
        if (item_type.maximum_stackability < instance.number_of_items())
            return false;
    }
    return true;
}

bool Reduction::lift_item_lengths_applies(const Instance& instance)
{
    // Single bin type (the bin's own length must be unambiguous), no fixed
    // items (a fixed item sitting inside the claimed "provably empty"
    // margin would be invisible to this purely 1D argument), and not
    // 'BinPackingWithLeftovers' (whose own leftover-length measurement is
    // exactly what growing an item's declared length would corrupt), and
    // not 'Knapsack' - see
    // this class's own doc comment. Unlike 'full_bin_reduction_applies',
    // not restricted to 'BinPacking' and no weight/resource exclusion:
    // nothing is ever hidden from the reduced instance here.
    return instance.number_of_bin_types() == 1
        && instance.bin_type(0).fixed_items.empty()
        && instance.objective() != Objective::BinPackingWithLeftovers
        && instance.objective() != Objective::Knapsack;
}

bool Reduction::remove_dominated_bin_types_applies(const Instance& instance)
{
    // Only objectives for which a bin type at least as long, at most as
    // costly and otherwise at least as permissive is always a valid
    // substitute: not 'BinPacking' (bin types must be used in their
    // declared order - see 'Solution::bin_type_order_feasible()' - so
    // removing one would make later ones available earlier than the
    // original instance allows), and not 'BinPackingWithLeftovers'/
    // 'Default' (both measure waste, which a longer substitute bin can
    // only increase).
    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::Feasibility
            && instance.objective() != Objective::VariableSizedBinPacking) {
        return false;
    }
    return instance.number_of_bin_types() > 1;
}

ItemPos Reduction::effective_copies_min(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id) const
{
    const ItemType& original_item_type = original_instance_->item_type(item_type_id);
    // For 'Knapsack', 'reduction_item_types[item_type_id].copies' can only
    // ever have shrunk below 'original_item_type.copies' via
    // 'remove_negative_profit_items' - copies trimmed away that way are
    // simply gone, never placed anywhere - so the requirement stays exactly
    // 'original_item_type.copies_min', just capped by whatever copies
    // genuinely remain (never below current 'copies', which would be
    // unsatisfiable). 'remove_negative_profit_items' only ever trims down
    // to exactly 'copies_min' itself, so this capping never actually
    // tightens anything in practice - it only guards the invariant.
    if (original_instance_->objective() == Objective::Knapsack) {
        return std::min(
                original_item_type.copies_min,
                reduction_item_types[item_type_id].copies);
    }
    ItemPos consumed = original_item_type.copies - reduction_item_types[item_type_id].copies;
    return std::max((ItemPos)0, original_item_type.copies_min - consumed);
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
        const ItemType& original_item_type = original_instance_->item_type(item_type_id);
        if (original_item_type.profit >= 0)
            continue;
        // Leave precedence-involved item types untouched - see this
        // class's own doc comment in 'reduction.hpp'.
        if (has_precedence(original_item_type))
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

bool Reduction::items_mergeable(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);

    if (item_type_1.length != item_type_2.length)
        return false;
    if (item_type_1.weight != item_type_2.weight
            || item_type_1.nesting_length != item_type_2.nesting_length
            || item_type_1.maximum_stackability != item_type_2.maximum_stackability
            || item_type_1.maximum_weight_after != item_type_2.maximum_weight_after
            || item_type_1.eligibility_id != item_type_2.eligibility_id)
        return false;
    // Profit is only compared for 'Knapsack' - see this class's own doc
    // comment in 'reduction.hpp' (mirrors rectangle's own
    // 'items_mergeable').
    if (original_instance_->objective() == Objective::Knapsack
            && item_type_1.profit != item_type_2.profit)
        return false;
    // Leave precedence-involved item types unmerged - see this class's own
    // doc comment in 'reduction.hpp'.
    if (has_precedence(item_type_1) || has_precedence(item_type_2))
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
    // which - see rectangle's own 'items_mergeable' doc comment for the
    // full soundness argument (identical here, no geometry involved).
    ItemPos effective_copies_min_1 = effective_copies_min(reduction_item_types, item_type_id_1);
    ItemPos effective_copies_min_2 = effective_copies_min(reduction_item_types, item_type_id_2);
    bool item_1_fully_mandatory = (effective_copies_min_1 == reduction_item_types[item_type_id_1].copies);
    bool item_2_fully_mandatory = (effective_copies_min_2 == reduction_item_types[item_type_id_2].copies);
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

BinPos Reduction::number_of_dedicated_bins() const
{
    BinPos total = 0;
    for (const FullBinItem& full_bin_item: full_bin_items_)
        total += full_bin_item.copies;
    for (const PerfectPair& pair: perfect_pairs_)
        total += pair.copies;
    for (const DominantSet& dominant_set: dominant_sets_)
        total += dominant_set.copies;
    return total;
}

bool Reduction::full_bin_reduction_item_ok(
        ItemTypeId item_type_id) const
{
    const ItemType& item_type = original_instance_->item_type(item_type_id);
    if (has_precedence(item_type))
        return false;
    // Single bin type (see 'full_bin_reduction_applies'), so bin type id 0
    // is the only one that matters.
    if (!original_instance_->item_type_fits_bin_type(item_type_id, 0))
        return false;
    return true;
}

bool Reduction::reduce_full_bin_items(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_length)
{
    const BinType& bin_type = original_instance_->bin_type(0);

    bool any_reduced = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed)
            continue;
        const ItemType& item_type = original_instance_->item_type(item_type_id);
        if (item_type.length != bin_length)
            continue;
        if (!full_bin_reduction_item_ok(item_type_id))
            continue;

        ItemPos copies = item.copies;
        if (bin_type.copies >= 0
                && number_of_dedicated_bins() + copies > bin_type.copies) {
            // Not enough bin copies left to reserve dedicated bins for
            // this item type: leave it for the underlying solver instead
            // (sound, just less effective - see 'reduce_perfect_pairs''s
            // identical guard for why).
            continue;
        }

        item.removed = true;
        full_bin_items_.push_back(FullBinItem{item_type_id, copies});
        any_reduced = true;
    }

    return any_reduced;
}

bool Reduction::reduce_perfect_pairs(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_length)
{
    const BinType& bin_type = original_instance_->bin_type(0);

    bool any_reduced = false;
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < (ItemTypeId)reduction_item_types.size();
            ++item_type_id_1) {
        ReductionItemType& item_1 = reduction_item_types[item_type_id_1];
        if (item_1.removed || item_1.copies == 0)
            continue;
        if (!full_bin_reduction_item_ok(item_type_id_1))
            continue;
        const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);

        // Two of this item type's own copies filling a bin together.
        if (2 * item_type_1.length == bin_length && item_1.copies >= 2) {
            ItemPos copies = item_1.copies / 2;
            if (!(bin_type.copies >= 0
                        && number_of_dedicated_bins() + copies > bin_type.copies)) {
                item_1.copies -= 2 * copies;
                if (item_1.copies == 0)
                    item_1.removed = true;
                perfect_pairs_.push_back(PerfectPair{item_type_id_1, item_type_id_1, copies});
                any_reduced = true;
                if (item_1.removed)
                    continue;
            }
        }

        for (ItemTypeId item_type_id_2 = item_type_id_1 + 1;
                item_type_id_2 < (ItemTypeId)reduction_item_types.size();
                ++item_type_id_2) {
            ReductionItemType& item_2 = reduction_item_types[item_type_id_2];
            if (item_2.removed)
                continue;
            const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);
            if (item_type_1.length + item_type_2.length != bin_length)
                continue;
            if (!full_bin_reduction_item_ok(item_type_id_2))
                continue;

            ItemPos copies = std::min(item_1.copies, item_2.copies);
            if (bin_type.copies >= 0
                    && number_of_dedicated_bins() + copies > bin_type.copies) {
                // Not enough bin copies left to reserve dedicated bins
                // for this pair: leave both item types for the
                // underlying solver instead (sound, just less
                // effective - this keeps the subtraction in
                // 'reduction_to_instance' from ever going negative).
                continue;
            }

            item_1.copies -= copies;
            if (item_1.copies == 0)
                item_1.removed = true;
            item_2.copies -= copies;
            if (item_2.copies == 0)
                item_2.removed = true;

            perfect_pairs_.push_back(PerfectPair{item_type_id_1, item_type_id_2, copies});
            any_reduced = true;
            break;
        }
    }

    return any_reduced;
}

bool Reduction::find_dominant_set(
        const std::vector<ReductionItemType>& reduction_item_types,
        const std::vector<ItemTypeId>& sorted_item_type_ids,
        ItemTypeId item_type_id,
        Length bin_length,
        std::vector<ItemTypeId>& dominant_set) const
{
    Length remaining_length = bin_length
        - original_instance_->item_type(item_type_id).length;
    dominant_set.clear();

    // Free copies of 'item_type_id', not counting the one being fixed.
    auto free_copies = [&](ItemTypeId other_item_type_id)
    {
        ItemPos copies = reduction_item_types[other_item_type_id].copies;
        if (other_item_type_id == item_type_id)
            --copies;
        return copies;
    };
    auto length = [&](ItemTypeId other_item_type_id)
    {
        return original_instance_->item_type(other_item_type_id).length;
    };

    // 'C': every free copy fitting beside the item being fixed. Sorted by
    // decreasing length, so 'C' is a suffix of 'sorted_item_type_ids',
    // starting at 'c_begin'.
    ItemPos c_begin = 0;
    while (c_begin < (ItemPos)sorted_item_type_ids.size()
            && length(sorted_item_type_ids[c_begin]) > remaining_length) {
        ++c_begin;
    }
    Length c_length_sum = 0;
    for (ItemPos pos = c_begin; pos < (ItemPos)sorted_item_type_ids.size(); ++pos) {
        ItemTypeId other_item_type_id = sorted_item_type_ids[pos];
        ItemPos copies = free_copies(other_item_type_id);
        if (copies <= 0)
            continue;
        c_length_sum += copies * length(other_item_type_id);
        if (c_length_sum > remaining_length)
            break;
    }

    // Rule 0: every copy of 'C' fits together beside the item: 'C' itself
    // contains every set that could ever share its bin.
    if (c_length_sum <= remaining_length) {
        for (ItemPos pos = c_begin; pos < (ItemPos)sorted_item_type_ids.size(); ++pos) {
            ItemTypeId other_item_type_id = sorted_item_type_ids[pos];
            for (ItemPos copy = 0; copy < free_copies(other_item_type_id); ++copy)
                dominant_set.push_back(other_item_type_id);
        }
        return true;
    }

    // Lengths of the 'number_of_lengths' shortest copies of 'C' strictly
    // longer than 'minimum_length', in increasing order.
    auto shortest_lengths = [&](
            ItemPos number_of_lengths,
            Length minimum_length)
    {
        std::vector<Length> lengths;
        for (ItemPos pos = (ItemPos)sorted_item_type_ids.size() - 1;
                pos >= c_begin && (ItemPos)lengths.size() < number_of_lengths;
                --pos) {
            ItemTypeId other_item_type_id = sorted_item_type_ids[pos];
            if (length(other_item_type_id) <= minimum_length)
                continue;
            for (ItemPos copy = 0;
                    copy < free_copies(other_item_type_id)
                    && (ItemPos)lengths.size() < number_of_lengths;
                    ++copy) {
                lengths.push_back(length(other_item_type_id));
            }
        }
        return lengths;
    };
    // 'true' iff no 'number_of_lengths' copies of 'C' strictly longer than
    // 'minimum_length' fit together beside the item.
    auto none_fit_together = [&](
            ItemPos number_of_lengths,
            Length minimum_length)
    {
        std::vector<Length> lengths = shortest_lengths(number_of_lengths, minimum_length);
        if ((ItemPos)lengths.size() < number_of_lengths)
            return true;
        Length length_sum = 0;
        for (Length l: lengths)
            length_sum += l;
        return length_sum > remaining_length;
    };

    // 'a': the longest copy of 'C'.
    ItemTypeId item_type_id_a = -1;
    for (ItemPos pos = c_begin; pos < (ItemPos)sorted_item_type_ids.size(); ++pos) {
        if (free_copies(sorted_item_type_ids[pos]) > 0) {
            item_type_id_a = sorted_item_type_ids[pos];
            break;
        }
    }
    Length length_a = length(item_type_id_a);

    // Rule 1: '{a}' dominates, either because 'a' fills the bin (anything
    // else fitting beside the item fits in 'a''s place too), or because no
    // two copies of 'C' fit together (anything else fitting beside the
    // item is a single copy, no longer than 'a').
    if (length_a == remaining_length || none_fit_together(2, -1)) {
        if (!full_bin_reduction_item_ok(item_type_id_a))
            return false;
        dominant_set.push_back(item_type_id_a);
        return true;
    }

    // Rule 2: '{a, b}', with 'b' the longest other copy of 'C' fitting
    // beside both the item and 'a'. It dominates when no three copies of
    // 'C' fit together (so anything else fitting beside the item is at most
    // a pair '{x, y}', 'x >= y'), and no two copies of 'C' strictly longer
    // than 'b' fit together (so 'y <= b', and 'x <= a' always, since 'a' is
    // the longest).
    ItemTypeId item_type_id_b = -1;
    for (ItemPos pos = c_begin; pos < (ItemPos)sorted_item_type_ids.size(); ++pos) {
        ItemTypeId other_item_type_id = sorted_item_type_ids[pos];
        ItemPos copies = free_copies(other_item_type_id);
        if (other_item_type_id == item_type_id_a)
            --copies;
        if (copies > 0 && length(other_item_type_id) <= remaining_length - length_a) {
            item_type_id_b = other_item_type_id;
            break;
        }
    }
    if (item_type_id_b == -1)
        return false;
    if (!none_fit_together(3, -1))
        return false;
    if (!none_fit_together(2, length(item_type_id_b)))
        return false;
    if (!full_bin_reduction_item_ok(item_type_id_a)
            || !full_bin_reduction_item_ok(item_type_id_b)) {
        return false;
    }
    dominant_set.push_back(item_type_id_a);
    dominant_set.push_back(item_type_id_b);
    return true;
}

bool Reduction::reduce_dominant_sets(
        std::vector<ReductionItemType>& reduction_item_types,
        bool shrink_bin)
{
    const BinType& bin_type = original_instance_->bin_type(0);

    // Free item types, by decreasing length.
    std::vector<ItemTypeId> sorted_item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        if (!reduction_item_types[item_type_id].removed
                && reduction_item_types[item_type_id].copies > 0) {
            sorted_item_type_ids.push_back(item_type_id);
        }
    }
    std::stable_sort(
            sorted_item_type_ids.begin(),
            sorted_item_type_ids.end(),
            [this](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return original_instance_->item_type(item_type_id_1).length
                    > original_instance_->item_type(item_type_id_2).length;
            });

    BinPos number_of_dedicated_bins = this->number_of_dedicated_bins();
    std::vector<ItemTypeId> dominant_set;
    bool any_reduced = false;
    // Fixing a bin only ever removes copies, which can only make a set
    // dominant for an item type that previously had none: repeat until
    // nothing changes.
    for (bool found = true; found;) {
        found = false;
        // Fixing bins only ever removes items, so the shrunk bin length
        // can only decrease from one pass to the next.
        Length bin_length = (shrink_bin)?
            shrunk_bin_length(reduction_item_types):
            bin_type.length;
        for (ItemTypeId item_type_id: sorted_item_type_ids) {
            while (reduction_item_types[item_type_id].copies > 0) {
                if (bin_type.copies >= 0
                        && number_of_dedicated_bins + 1 > bin_type.copies) {
                    // Not enough bin copies left to reserve another
                    // dedicated bin: leave the remaining items for the
                    // underlying solver instead (same guard as
                    // 'reduce_full_bin_items'/'reduce_perfect_pairs').
                    return any_reduced;
                }
                if (!full_bin_reduction_item_ok(item_type_id))
                    break;
                if (!find_dominant_set(
                            reduction_item_types,
                            sorted_item_type_ids,
                            item_type_id,
                            bin_length,
                            dominant_set)) {
                    break;
                }

                dominant_set.insert(dominant_set.begin(), item_type_id);
                for (ItemTypeId set_item_type_id: dominant_set) {
                    ReductionItemType& item = reduction_item_types[set_item_type_id];
                    item.copies--;
                    if (item.copies == 0)
                        item.removed = true;
                }
                if (!dominant_sets_.empty()
                        && dominant_sets_.back().item_type_ids == dominant_set) {
                    dominant_sets_.back().copies++;
                } else {
                    dominant_sets_.push_back(DominantSet{dominant_set, 1});
                }
                number_of_dedicated_bins++;
                found = true;
                any_reduced = true;
            }
        }
    }

    return any_reduced;
}

Length Reduction::max_achievable_length_sum(
        const std::vector<ReductionItemType>& reduction_item_types,
        Length capacity,
        ItemTypeId excluded_item_type_id) const
{
    subsetsumsolver::InstanceBuilder sss_instance_builder;
    sss_instance_builder.set_capacity(capacity);
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        // Not 'item.removed': an item type merged into another one (see
        // 'merge_identical_items') is 'removed' too, but its copies are
        // still genuinely, physically there - only 'reduction_to_instance'
        // ever folds them into their survivor's own count. Every other
        // reason an item type can be 'removed' already leaves it with
        // 'copies == 0', so checking 'copies' alone already excludes it
        // correctly, without needing to special-case 'removed' at all.
        ItemPos copies = item.copies;
        if (item_type_id == excluded_item_type_id)
            --copies;
        if (copies <= 0)
            continue;
        const ItemType& item_type = original_instance_->item_type(item_type_id);
        // The bound must never *underestimate* what is truly achievable
        // (that is the direction that would make 'lift_item_lengths'
        // unsound), so every item type counts: a nesting one at its
        // smallest possible footprint (it takes 'length - nesting_length'
        // whenever it is not the first item in its bin), and one with a
        // finite 'maximum_weight_after' at its raw length (that constraint
        // only ever rules combinations out).
        Length length = item.length - item_type.nesting_length;
        if (length <= 0 || length > capacity)
            continue;
        // No subset within 'capacity' can ever use more copies than this.
        copies = (std::min)(copies, (ItemPos)(capacity / length));
        for (ItemPos copy = 0; copy < copies; ++copy)
            sss_instance_builder.add_item(length);
    }

    subsetsumsolver::Instance sss_instance = sss_instance_builder.build();
    subsetsumsolver::Parameters sss_parameters;
    sss_parameters.verbosity_level = 0;
    auto sss_output = subsetsumsolver::dynamic_programming_bellman_word_ram(sss_instance, sss_parameters);
    return sss_output.bound;
}

Length Reduction::shrunk_bin_length(
        const std::vector<ReductionItemType>& reduction_item_types) const
{
    // Equation (7) of rectangle's own 'compute_shrunk_bin_sizes': no bin
    // can ever hold more than the largest achievable combination of the
    // remaining items' lengths.
    return max_achievable_length_sum(
            reduction_item_types,
            original_instance_->bin_type(0).length,
            -1);
}

bool Reduction::lift_item_lengths(
        std::vector<ReductionItemType>& reduction_item_types,
        Length bin_length)
{

    bool found = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        ReductionItemType& item = reduction_item_types[item_type_id];
        if (item.removed || item.copies != 1)
            continue;

        // A survivor that some other item type was merged into (see
        // 'merge_identical_items') really ends up with more than its own
        // single copy once 'reduction_to_instance' folds the merged-away
        // copies back in - checked here (rather than by running ahead of
        // 'merge_identical_items') since 'merge_identical_items' itself
        // never updates 'copies' at all (only 'reduction_to_instance' does,
        // at the very end): growing this item type's length here as if it
        // were a genuine singleton would then wrongly stop *all* of its
        // final copies from ever sharing a bin with each other, even
        // though their true (smaller) length might allow it.
        bool is_merge_target = false;
        for (const ReductionItemType& other: reduction_item_types) {
            if (other.merged_into == item_type_id) {
                is_merge_target = true;
                break;
            }
        }
        if (is_merge_target)
            continue;

        const ItemType& item_type = original_instance_->item_type(item_type_id);
        if (item_type.nesting_length != 0)
            continue;
        if (item_type.maximum_weight_after != std::numeric_limits<Weight>::infinity())
            continue;

        Length capacity = bin_length - item.length;
        if (capacity <= 0)
            continue;

        Length achievable_other = max_achievable_length_sum(
                reduction_item_types, capacity, item_type_id);
        if (achievable_other >= capacity)
            continue;

        item.length = bin_length - achievable_other;
        found = true;
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

    // Unlike item dominance, bin dominance runs towards the *bigger* one:
    // A must be able to hold anything B could.
    if (bin_type_a.length < bin_type_b.length)
        return false;
    if (bin_type_a.cost > bin_type_b.cost)
        return false;
    if (bin_type_a.maximum_weight < bin_type_b.maximum_weight)
        return false;

    // Fixed items are a per-bin-type feature this class does not attempt
    // to compare across two different bin types - same as rectangle's own
    // 'bin_type_dominates'.
    if (!bin_type_a.fixed_items.empty() || !bin_type_b.fixed_items.empty())
        return false;

    // Resources: 'resource_id' carries no meaning across two different bin
    // types, so any resource of A's own blocks dominance. B alone having
    // resources is fine (A, without any, is unrestricted along every
    // resource dimension), except a 'penalize' resource with a negative
    // 'penalty' (a profit bonus A could never replicate) - see rectangle's
    // own 'bin_type_dominates'.
    if (bin_type_a.number_of_resources() > 0)
        return false;
    for (ResourceId resource_id = 0;
            resource_id < bin_type_b.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type_b.resource(resource_id);
        if (resource.penalize && resource.penalty < 0)
            return false;
    }

    // Eligibility superset: A must support every eligibility id B does.
    for (EligibilityId eligibility_id: bin_type_b.eligibility_ids) {
        if (std::find(
                    bin_type_a.eligibility_ids.begin(),
                    bin_type_a.eligibility_ids.end(),
                    eligibility_id)
                == bin_type_a.eligibility_ids.end())
            return false;
    }

    return true;
}

std::vector<bool> Reduction::compute_dominated_bin_types() const
{
    std::vector<bool> dominated(original_instance_->number_of_bin_types(), false);
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
            // could use, on top of its own: a solution never needs more
            // non-empty bins than items, and its only empty bins of A are
            // the ones A's own 'copies_min' forces.
            const BinType& bin_type_a = original_instance_->bin_type(bin_type_id_a);
            if (bin_type_a.copies >= 0
                    && bin_type_a.copies
                    < original_instance_->number_of_items() + bin_type_a.copies_min) {
                continue;
            }
            if (!bin_type_dominates(bin_type_id_a, bin_type_id_b))
                continue;
            dominated[bin_type_id_b] = true;
            break;
        }
    }
    return dominated;
}

Instance Reduction::reduction_to_instance(
        const std::vector<ReductionItemType>& reduction_item_types,
        const std::vector<bool>& bin_type_removed)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(original_instance_->objective());
    instance_builder.set_parameters(original_instance_->parameters());
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    BinPos number_of_dedicated_bins = this->number_of_dedicated_bins();
    ItemPos remaining_items = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        // A merged-away item type (see 'merge_identical_items') is
        // 'removed' too, but its copies still need bin capacity in the
        // reduced instance - just folded into its survivor's own count
        // below - unlike every other reason an item type can be 'removed'
        // (consumed into a dedicated bin via 'reduce_full_bin_items'/
        // 'reduce_perfect_pairs'), which genuinely need none.
        if (!reduction_item_types[item_type_id].removed
                || reduction_item_types[item_type_id].merged_into != -1)
            remaining_items += original_instance_->item_type(item_type_id).copies;
    }

    reduced_to_original_bin_type_id_.clear();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        // Dominated by another bin type with enough copies to cover any use
        // of this one (see 'compute_dominated_bin_types'): left out of the
        // reduced instance entirely. 'add_item_type' below then only copies
        // resource consumptions for the bin types that were added.
        if (bin_type_id < (BinTypeId)bin_type_removed.size()
                && bin_type_removed[bin_type_id]) {
            continue;
        }
        BinTypeId new_bin_type_id = instance_builder.add_bin_type(*original_instance_, bin_type_id);
        reduced_to_original_bin_type_id_.push_back(bin_type_id);
        if (number_of_dedicated_bins > 0) {
            // Fold the dedicated bins' capacity out of the reduced
            // instance's own bin type copies: they are entirely absent
            // from this instance (see 'FullBinItem'/'PerfectPair'), so
            // nothing here should ever be allowed to use their reserved
            // capacity. Only bin type id 0 is ever affected - see
            // 'full_bin_reduction_applies' (a single bin type is one of
            // its own preconditions). 'reduce_full_bin_items'/
            // 'reduce_perfect_pairs' never reserve more than
            // 'bin_type.copies' itself, so this subtraction never goes
            // negative - but it can reach exactly 0, which
            // 'InstanceBuilder' rejects outright ('copies' must be > 0 or
            // -1), so that case needs its own handling below.
            const BinType& original_bin_type = original_instance_->bin_type(bin_type_id);
            if (original_bin_type.copies >= 0) {
                BinPos new_copies = original_bin_type.copies - number_of_dedicated_bins;
                if (new_copies > 0) {
                    instance_builder.set_bin_type_copies(new_bin_type_id, new_copies);
                } else if (remaining_items == 0) {
                    // Zero bin capacity left, but also nothing left that
                    // could ever need it (every item type was itself
                    // consumed by a dedicated-bin reservation): leave the
                    // bin type's copies at its original (nonzero) value -
                    // harmless, since a solve over zero items never
                    // touches bin capacity at all.
                } else {
                    // Zero bin capacity left, with real items still
                    // needing to be packed: the original instance needs
                    // strictly more bins than this bin type has copies
                    // for, so it is infeasible outright. Record it and
                    // leave a harmless nonzero placeholder so the
                    // instance still builds; the recursive solve on it is
                    // never actually reached (see 'optimize()', which
                    // checks 'proven_infeasible()' first).
                    proven_infeasible_ = true;
                }
            }
        }
    }

    reduced_item_origin_runs_.clear();
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
        // A merged-away item type (see 'merge_identical_items') is
        // 'removed' too, but its copies still need to be folded into its
        // survivor's own count below - unlike a fully trimmed-away item
        // type ('remove_negative_profit_items'), which genuinely
        // contributes nothing.
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

        // 'add_item_type(original_instance, item_type_id)' already copies
        // every other field (length, weight, nesting length, maximum
        // stackability, maximum weight after, eligibility, resource
        // consumption, profit) as-is, and resolves this item type's own
        // precedences (if any - though 'items_mergeable'/
        // 'remove_negative_profit_items' never touch a precedence-involved
        // item type, so it always survives here unchanged, at its original
        // copies). 'set_item_type_length' below then overrides the length
        // with 'item.length' - the original length unless 'lift_item_lengths'
        // grew it, in which case this is the only place that grown length
        // ever reaches the reduced instance the downstream solve sees.
        ItemTypeId new_item_type_id = instance_builder.add_item_type(*original_instance_, item_type_id);
        instance_builder.set_item_type_length(new_item_type_id, item.length);
        instance_builder.set_item_type_copies(new_item_type_id, total_copies);
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
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
    // item types (see 'ReductionItemType'). Always built and compacted back
    // via 'reduction_to_instance' at the end (even when 'parameters.reduce'
    // is 'false', in which case it is an identity rebuild): this keeps
    // 'reduced_item_origin_runs_' always populated, so 'unreduce_solution'
    // never needs a separate no-op code path - mirrors rectangle's own
    // 'Reduction' constructor.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
        reduction_item_types[item_type_id].length = instance.item_type(item_type_id).length;
    }

    if (parameters.reduce) {
        if (parameters.remove_negative_profit_items
                && remove_negative_profit_items_applies(instance)) {
            remove_negative_profit_items(reduction_item_types);
        }
    }

    // A classical knapsack is solved exactly by
    // 'knapsacksolver::dynamic_programming_primal_dual', which already
    // performs every useful reduction itself, far more efficiently: only
    // 'remove_negative_profit_items' above is applied then.
    if (parameters.reduce && !is_classical_knapsack(instance)) {
        // The bin length every operation below reasons against: the
        // shrunk one (see 'shrunk_bin_length') when enabled, recomputed
        // before each operation from the items still remaining, since
        // every reservation can only shrink it further.
        auto bin_length = [&]()
        {
            return (parameters.shrink_bin)?
                shrunk_bin_length(reduction_item_types):
                instance.bin_type(0).length;
        };
        // Run ahead of 'merge_identical_items': consuming an item type
        // entirely into a dedicated-bin reservation here saves that (now
        // moot) merge check from ever considering it.
        if (full_bin_reduction_applies(instance)) {
            if (parameters.reduce_full_bin_items)
                reduce_full_bin_items(reduction_item_types, bin_length());
            if (parameters.reduce_perfect_pairs)
                reduce_perfect_pairs(reduction_item_types, bin_length());
            if (parameters.reduce_dominant_sets)
                reduce_dominant_sets(reduction_item_types, parameters.shrink_bin);
        }
        if (parameters.merge_identical_items)
            merge_identical_items(reduction_item_types);
        // Runs *after* 'merge_identical_items' (unlike the dedicated-bin
        // reservations above, which run *before* it): a merge happening
        // *after* a lift could fold a just-lifted singleton together with
        // another item type into one, final, more-than-one-copy survivor
        // sharing that one grown length - exactly the unsound case
        // 'lift_item_lengths''s own 'copies == 1' check exists to rule
        // out. Running lift last means every merge that could ever
        // disqualify a candidate has already happened by the time it
        // checks for one (see 'lift_item_lengths' for that check, since
        // 'merge_identical_items' itself never updates a survivor's own
        // 'copies' - only 'reduction_to_instance' folds the merged-away
        // copies in, at the very end).
        if (parameters.lift_item_lengths && lift_item_lengths_applies(instance))
            lift_item_lengths(reduction_item_types, bin_length());
    }

    // Bin types are never touched by any other operation, so this only
    // needs computing once, and is only consumed by
    // 'reduction_to_instance'.
    std::vector<bool> bin_type_removed;
    if (parameters.reduce
            && !is_classical_knapsack(instance)
            && parameters.remove_dominated_bin_types
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
        // Identity unless 'compute_dominated_bin_types' left some bin types
        // out of 'instance_'.
        BinTypeId original_bin_type_id
            = reduced_to_original_bin_type_id_[solution_bin.bin_type_id];

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
                BinPos new_bin_pos = solution_builder.add_bin(original_bin_type_id, batch);
                for (const SolutionItem& solution_item: solution_bin.items) {
                    ItemTypeId original_item_type_id
                        = remaining_runs[solution_item.item_type_id].back().item_type_id;
                    solution_builder.add_item(new_bin_pos, original_item_type_id);
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
                BinPos new_bin_pos = solution_builder.add_bin(original_bin_type_id, 1);
                for (const SolutionItem& solution_item: solution_bin.items) {
                    ItemTypeId original_item_type_id
                        = consume_one_origin(remaining_runs[solution_item.item_type_id]);
                    solution_builder.add_item(new_bin_pos, original_item_type_id);
                }
                remaining_bin_copies--;
            }
        }

        for (ItemTypeId reduced_item_type_id: touched_types)
            slot_count[reduced_item_type_id] = 0;
    }

    // Reinstate each "full bin item"/"perfect pair"/"dominant set"
    // reservation as its own dedicated bin(s) (see 'FullBinItem'/
    // 'PerfectPair'/'DominantSet'): entirely absent
    // from 'instance_', so nothing above ever encounters them while
    // scanning 'solution'. Only ever populated when 'original_instance_'
    // has a single bin type (see 'full_bin_reduction_applies'), so bin
    // type id 0 always refers to it. Every reservation of a given kind
    // shares the exact same pattern across all of its copies (no
    // per-copy geometry to vary, unlike rectangle's own version), so one
    // 'add_bin' call per reservation - not one per copy - already covers
    // all of them.
    for (const FullBinItem& full_bin_item: full_bin_items_) {
        BinPos new_bin_pos = solution_builder.add_bin(0, full_bin_item.copies);
        solution_builder.add_item(new_bin_pos, full_bin_item.item_type_id);
    }
    for (const PerfectPair& pair: perfect_pairs_) {
        BinPos new_bin_pos = solution_builder.add_bin(0, pair.copies);
        solution_builder.add_item(new_bin_pos, pair.item_type_id_1);
        solution_builder.add_item(new_bin_pos, pair.item_type_id_2);
    }
    for (const DominantSet& dominant_set: dominant_sets_) {
        BinPos new_bin_pos = solution_builder.add_bin(0, dominant_set.copies);
        for (ItemTypeId item_type_id: dominant_set.item_type_ids)
            solution_builder.add_item(new_bin_pos, item_type_id);
    }

    return solution_builder.build();
}
