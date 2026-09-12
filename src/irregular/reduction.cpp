#include "packingsolver/irregular/reduction.hpp"

#include "packingsolver/irregular/instance_builder.hpp"

#include "irregular/solution_builder.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * 'true' iff 'shapes_1' and 'shapes_2' describe the exact same sequence of
 * (shape, quality rule) sub-shapes. Relies on 'shape::operator==' for the
 * geometric comparison - an exact, order-sensitive structural equality, so
 * this is a conservative (sound but not complete) notion of "same
 * footprint": two item types with congruent but differently represented
 * shapes are missed, only costing a missed merge opportunity, never
 * correctness.
 */
bool item_shapes_equal(
        const std::vector<ItemShape>& shapes_1,
        const std::vector<ItemShape>& shapes_2)
{
    if (shapes_1.size() != shapes_2.size())
        return false;
    for (ItemShapePos shape_pos = 0;
            shape_pos < (ItemShapePos)shapes_1.size();
            ++shape_pos) {
        if (shapes_1[shape_pos].quality_rule != shapes_2[shape_pos].quality_rule)
            return false;
        if (!(shapes_1[shape_pos].shape_orig == shapes_2[shape_pos].shape_orig))
            return false;
    }
    return true;
}

/**
 * 'true' iff 'rotations_1' and 'rotations_2' allow the exact same set of
 * (angle range, mirror) entries, in the same order - any consistent order
 * works since two mergeable item types are only ever swapped for one
 * another wholesale, never partially.
 */
bool allowed_rotations_equal(
        const std::vector<AllowedRotation>& rotations_1,
        const std::vector<AllowedRotation>& rotations_2)
{
    if (rotations_1.size() != rotations_2.size())
        return false;
    for (size_t rotation_pos = 0; rotation_pos < rotations_1.size(); ++rotation_pos) {
        const AllowedRotation& rotation_1 = rotations_1[rotation_pos];
        const AllowedRotation& rotation_2 = rotations_2[rotation_pos];
        if (rotation_1.start_angle != rotation_2.start_angle
                || rotation_1.end_angle != rotation_2.end_angle
                || rotation_1.mirror != rotation_2.mirror)
            return false;
    }
    return true;
}

/**
 * Same helper (same name/semantics) as 'rectangle::Reduction''s own
 * anonymous-namespace 'find_item_type_schedule' - reimplemented locally
 * here since it is not shared/exported.
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

bool Reduction::items_mergeable(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);

    if (!item_shapes_equal(item_type_1.shapes, item_type_2.shapes))
        return false;
    if (!allowed_rotations_equal(item_type_1.allowed_rotations, item_type_2.allowed_rotations))
        return false;

    // Profit is only compared for 'Knapsack' - see
    // 'rectangle::Reduction::items_mergeable''s own doc comment for the
    // full argument (every other objective here restores each copy's true
    // original profit in 'unreduce_solution' regardless of merging, but
    // 'Knapsack' optimizes profit directly over a solve that may leave
    // copies unplaced, so a profit mismatch must block the merge outright).
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
        }
    }

    // Each candidate's own current copies must be either entirely
    // mandatory or entirely optional, and both candidates must agree on
    // which - see 'rectangle::Reduction::items_mergeable''s own doc
    // comment for the concrete counterexample this rules out.
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

    // Bin types are copied 1:1, in order - no bin-type removal in this
    // scope - so the reduced instance's own bin type ids always coincide
    // with 'original_instance_''s, unlike item type ids (which skip
    // removed/merged-away entries below).
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

        // 'add_item_type(original_instance, id)' already copies every
        // field that never changes here (shapes, allowed rotations,
        // profit, resource consumption schedules, cached periodic
        // packings, ...) - only 'copies'/'copies_min' need overriding for
        // a merged survivor.
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
    // item types (see 'ReductionItemType'). Always built and compacted
    // back via 'reduction_to_instance' at the end (even when
    // 'parameters.reduce' is 'false', in which case it is an identity
    // rebuild): this keeps 'reduced_copy_origins_' always populated, so
    // 'unreduce_solution' never needs a separate no-op code path.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
    }

    if (!parameters.reduce) {
        instance_ = reduction_to_instance(reduction_item_types);
        return;
    }

    // Negative-profit trimming runs first: it can only ever shrink a
    // candidate's own copies (never affect its shape/profit/eligibility),
    // which 'merge_identical_items''s own mandatory-vs-optional check
    // needs to see already applied. Each op is only ever needed once: an
    // item type's own profit sign never changes, and a merge never creates
    // a new negative-profit or newly-identical item type that wasn't
    // already there to find in one pass.
    if (remove_negative_profit_items_applies(instance) && parameters.remove_negative_profit_items)
        remove_negative_profit_items(reduction_item_types);

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
    // given placement actually is (see 'merge_identical_items').
    std::vector<ItemPos> next_copy_index(instance_.number_of_item_types(), 0);

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        // One 'SolutionBin' entry with 'copies > 1' means several
        // *distinct* physical bins share one identical placement pattern -
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
            for (const SolutionItem& solution_item: solution_bin.items) {
                ItemPos reduced_copy_index = next_copy_index[solution_item.item_type_id]++;
                const CopyOrigin& origin
                    = reduced_copy_origins_[solution_item.item_type_id][reduced_copy_index];
                solution_builder.add_item(
                        new_bin_pos,
                        origin.item_type_id,
                        solution_item.bl_corner,
                        solution_item.angle,
                        solution_item.mirror,
                        solution_item.is_fixed);
            }
        }
    }

    return solution_builder.build();
}
