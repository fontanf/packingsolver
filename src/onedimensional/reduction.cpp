#include "packingsolver/onedimensional/reduction.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "onedimensional/solution_builder.hpp"

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

Instance Reduction::reduction_to_instance(
        const std::vector<ReductionItemType>& reduction_item_types)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(original_instance_->objective());
    instance_builder.set_parameters(original_instance_->parameters());
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    for (BinTypeId bin_type_id = 0;
            bin_type_id < original_instance_->number_of_bin_types();
            ++bin_type_id) {
        instance_builder.add_bin_type(*original_instance_, bin_type_id);
    }

    reduced_copy_origins_.clear();
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

        // Per-copy origin list for this survivor: its own copies, then, in
        // discovery order, every item type merged into it (see
        // 'merge_identical_items') - any consistent order works, since
        // merged item types are interchangeable by construction (see
        // 'items_mergeable').
        std::vector<CopyOrigin> copy_origins;
        ItemPos total_copies_min = effective_copies_min(reduction_item_types, item_type_id);
        for (ItemPos copy_index = 0; copy_index < item.copies; ++copy_index)
            copy_origins.push_back({item_type_id, copy_index});
        for (ItemTypeId other_item_type_id = 0;
                other_item_type_id < (ItemTypeId)reduction_item_types.size();
                ++other_item_type_id) {
            if (reduction_item_types[other_item_type_id].merged_into != item_type_id)
                continue;
            ItemPos merged_copies = reduction_item_types[other_item_type_id].copies;
            total_copies_min += effective_copies_min(reduction_item_types, other_item_type_id);
            for (ItemPos copy_index = 0; copy_index < merged_copies; ++copy_index)
                copy_origins.push_back({other_item_type_id, copy_index});
        }

        // 'add_item_type(original_instance, item_type_id)' already copies
        // every other field (weight, nesting length, maximum stackability,
        // maximum weight after, eligibility, resource consumption, profit)
        // as-is, and resolves this item type's own precedences (if any -
        // though 'items_mergeable'/'remove_negative_profit_items' never
        // touch a precedence-involved item type, so it always survives
        // here unchanged, at its original copies).
        ItemTypeId new_item_type_id = instance_builder.add_item_type(*original_instance_, item_type_id);
        instance_builder.set_item_type_copies(new_item_type_id, (ItemPos)copy_origins.size());
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
        reduced_copy_origins_.push_back(std::move(copy_origins));
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
    // 'reduced_copy_origins_' always populated, so 'unreduce_solution'
    // never needs a separate no-op code path - mirrors rectangle's own
    // 'Reduction' constructor.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
    }

    if (parameters.reduce) {
        if (parameters.remove_negative_profit_items
                && remove_negative_profit_items_applies(instance)) {
            remove_negative_profit_items(reduction_item_types);
        }
        if (parameters.merge_identical_items)
            merge_identical_items(reduction_item_types);
    }

    instance_ = reduction_to_instance(reduction_item_types);
}

Solution Reduction::unreduce_solution(
        const Solution& solution) const
{
    SolutionBuilder solution_builder(*original_instance_);

    // For each reduced item type, how many of its placements have been
    // encountered so far while scanning 'solution' - indexes
    // 'reduced_copy_origins_' to resolve which original item type/copy a
    // given placement actually is (see 'merge_identical_items'). Any
    // consistent order works, since a reduced item type's copies are
    // interchangeable by construction.
    std::vector<ItemPos> next_copy_index(instance_.number_of_item_types(), 0);

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        // Each of 'solution_bin''s 'copies' physical bins gets its own new
        // bin here (rather than one new bin with 'copies' set directly):
        // after a merge, two physical repetitions of the exact same
        // reduced-instance pattern can resolve to *different* original
        // item types (e.g. one repetition's copy of merged item type X
        // resolves back to original type A, another's to original type
        // B), so they can no longer share one single bin entry the way the
        // reduced solution does.
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy) {
            BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, 1);
            for (const SolutionItem& solution_item: solution_bin.items) {
                ItemPos reduced_copy_index = next_copy_index[solution_item.item_type_id]++;
                const CopyOrigin& origin
                    = reduced_copy_origins_[solution_item.item_type_id][reduced_copy_index];
                solution_builder.add_item(new_bin_pos, origin.item_type_id);
            }
        }
    }

    return solution_builder.build();
}
