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

    reduced_copy_origins_.clear();
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)reduction_item_types.size();
            ++item_type_id) {
        const ReductionItemType& item = reduction_item_types[item_type_id];
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
        // One 'SolutionBin' entry with 'copies > 1' means several
        // *distinct* physical bins share one identical stacking pattern -
        // each of those physical bins still holds its own distinct
        // original items, so each needs its own independent pass over
        // 'next_copy_index' (a merged reduced item type occurring once in
        // this pattern must draw a *different* original copy for each of
        // the 'copies' physical bins, never the same one reused - the
        // built 'Solution' has no way to record "copies physical bins,
        // same pattern, but different per-copy item ids" any other way
        // than as 'copies' separate one-copy bin entries). Mirrors
        // 'rectangle::Reduction::unreduce_solution''s own per-copy loop.
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy) {
            BinPos new_bin_pos = solution_builder.add_bin(solution_bin.bin_type_id, 1);
            for (const SolutionStack& solution_stack: solution_bin.stacks) {
                StackId new_stack_id = solution_builder.add_stack(
                        new_bin_pos,
                        solution_stack.x_start,
                        solution_stack.x_end,
                        solution_stack.y_start,
                        solution_stack.y_end);
                for (const SolutionItem& solution_item: solution_stack.items) {
                    ItemPos reduced_copy_index = next_copy_index[solution_item.item_type_id]++;
                    const CopyOrigin& origin
                        = reduced_copy_origins_[solution_item.item_type_id][reduced_copy_index];
                    solution_builder.add_item(
                            new_bin_pos,
                            new_stack_id,
                            origin.item_type_id,
                            solution_item.rotation);
                }
            }
        }
    }

    return solution_builder.build();
}
