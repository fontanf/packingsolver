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
        // every other field (weight, nesting length, maximum stackability,
        // maximum weight after, eligibility, resource consumption, profit)
        // as-is, and resolves this item type's own precedences (if any -
        // though 'items_mergeable'/'remove_negative_profit_items' never
        // touch a precedence-involved item type, so it always survives
        // here unchanged, at its original copies).
        ItemTypeId new_item_type_id = instance_builder.add_item_type(*original_instance_, item_type_id);
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
                BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, batch);
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
                BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, 1);
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

    return solution_builder.build();
}
