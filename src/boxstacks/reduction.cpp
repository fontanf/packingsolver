#include "packingsolver/boxstacks/reduction.hpp"

#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/solution_builder.hpp"

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

bool Reduction::items_mergeable(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);

    if (item_type_1.box.x != item_type_2.box.x
            || item_type_1.box.y != item_type_2.box.y
            || item_type_1.box.z != item_type_2.box.z)
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

    if (!same_rotations(item_type_1.rotations, item_type_2.rotations))
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
    }

    if (parameters.remove_negative_profit_items
            && remove_negative_profit_items_applies(instance)) {
        remove_negative_profit_items(reduction_item_types);
    }

    if (parameters.merge_identical_items)
        merge_identical_items(reduction_item_types);

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
                BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, batch);
                for (const SolutionStack& solution_stack: solution_bin.stacks) {
                    StackId new_stack_id = solution_builder.add_stack(
                            new_bin_pos,
                            solution_stack.x_start,
                            solution_stack.x_end,
                            solution_stack.y_start,
                            solution_stack.y_end);
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
                BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, 1);
                for (const SolutionStack& solution_stack: solution_bin.stacks) {
                    StackId new_stack_id = solution_builder.add_stack(
                            new_bin_pos,
                            solution_stack.x_start,
                            solution_stack.x_end,
                            solution_stack.y_start,
                            solution_stack.y_end);
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
