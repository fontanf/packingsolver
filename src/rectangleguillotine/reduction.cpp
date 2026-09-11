#include "packingsolver/rectangleguillotine/reduction.hpp"

#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

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

void Reduction::remove_negative_profit_items(
        std::vector<ReductionItemType>& reduction_item_types) const
{
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
    }
}

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

bool Reduction::items_mergeable(
        const std::vector<ReductionItemType>& reduction_item_types,
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2) const
{
    const ItemType& item_type_1 = original_instance_->item_type(item_type_id_1);
    const ItemType& item_type_2 = original_instance_->item_type(item_type_id_2);

    if (item_type_1.rect.w != item_type_2.rect.w || item_type_1.rect.h != item_type_2.rect.h)
        return false;
    if (item_type_1.oriented != item_type_2.oriented)
        return false;

    // Profit is only compared for 'Knapsack' - see 'items_mergeable''s own
    // doc comment (and 'rectangle::Reduction::items_mergeable''s, for the
    // full argument).
    if (original_instance_->objective() == Objective::Knapsack
            && item_type_1.profit != item_type_2.profit)
        return false;

    // Both must be alone in their own stack - see 'items_mergeable''s own
    // doc comment for why a shared stack blocks the merge.
    if (original_instance_->stack_size(item_type_1.stack_id) != 1
            || original_instance_->stack_size(item_type_2.stack_id) != 1)
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
        const std::vector<ReductionItemType>& reduction_item_types)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(original_instance_->objective());
    instance_builder.set_parameters(original_instance_->parameters());
    instance_builder.set_feasibility_callback(original_instance_->feasibility_callback());

    // Every bin type is copied over unchanged - no bin type removal in
    // this two-operation class.
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

        // Per-copy origin list for this survivor: its own copies, then,
        // in discovery order, every item type merged into it (see
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

        const ItemType& original_item_type = original_instance_->item_type(item_type_id);
        ItemTypeId new_item_type_id = instance_builder.add_item_type(
                original_item_type.rect.w,
                original_item_type.rect.h,
                original_item_type.oriented,
                original_item_type.stack_id);
        instance_builder.set_item_type_profit(new_item_type_id, original_item_type.profit);
        instance_builder.set_item_type_copies(new_item_type_id, (ItemPos)copy_origins.size());
        instance_builder.set_item_type_copies_min(new_item_type_id, total_copies_min);
        // Copy resource consumptions (the bin types themselves, including
        // their resources, were already copied above via 'add_bin_type(
        // *original_instance_, bin_type_id)', so ids still line up 1:1
        // with 'original_instance_' - no bin type was ever skipped here).
        for (BinTypeId bin_type_id = 0;
                bin_type_id < original_instance_->number_of_bin_types();
                ++bin_type_id) {
            const BinType& original_bin_type = original_instance_->bin_type(bin_type_id);
            for (const ItemResourceConsumption& consumption: original_item_type.resources[bin_type_id]) {
                const std::vector<double>& schedule
                    = original_bin_type.resource(consumption.resource_id)
                        .item_consumptions[consumption.consumption_pos].second;
                instance_builder.add_resource_consumption(
                        bin_type_id,
                        consumption.resource_id,
                        new_item_type_id,
                        schedule);
            }
        }
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
    // 'parameters.reduce' is 'false' below, in which case it is an
    // identity rebuild): this keeps 'reduced_copy_origins_' always
    // populated, so 'unreduce_solution' never needs a separate no-op code
    // path.
    std::vector<ReductionItemType> reduction_item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        reduction_item_types[item_type_id].copies = instance.item_type(item_type_id).copies;
    }

    if (parameters.reduce) {
        if (remove_negative_profit_items_applies(instance)
                && parameters.remove_negative_profit_items) {
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
        // One 'SolutionBin' entry with 'copies > 1' means several
        // *distinct* physical bins share one identical cutting pattern -
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
            solution_builder.add_bin(
                    solution_bin.bin_type_id,
                    1,
                    solution_bin.first_cut_orientation);
            // Replay every node of the cut tree exactly as-is (same
            // depths, same cut positions - see 'SolutionBuilder::add_node'
            // 's own doc comment: nodes are appended as the last child of
            // the last node added at the parent depth, so replaying
            // 'solution_bin.nodes' in their own stored order - already the
            // order they were built in - reconstructs an identical tree),
            // substituting each leaf's reduced item type id for its true
            // original one.
            //
            // Which of 'r'/'t' a given depth's cut position actually is
            // depends on 'solution_bin.first_cut_orientation', not on
            // depth parity alone - see 'SolutionBuilder::add_node''s own
            // condition, which this mirrors exactly.
            // ('InstanceFlipper::unflip_solution' gets away with the
            // depth-parity-only shortcut only because it always forces the
            // flipped instance's own first cut to 'Vertical', so
            // 'first_cut_orientation' is never actually a free variable
            // there - not true here, since this reduction never touches
            // cut orientation and the instance may leave it unconstrained
            // ('Any').)
            for (const SolutionNode& node: solution_bin.nodes) {
                if (node.d <= 0)
                    continue;
                bool cut_along_x =
                    (solution_bin.first_cut_orientation == CutOrientation::Vertical && node.d % 2 == 1)
                    || (solution_bin.first_cut_orientation == CutOrientation::Horizontal && node.d % 2 == 0);
                if (cut_along_x) {
                    solution_builder.add_node(node.d, node.r);
                } else {
                    solution_builder.add_node(node.d, node.t);
                }
                if (node.item_type_id >= 0) {
                    ItemPos reduced_copy_index = next_copy_index[node.item_type_id]++;
                    const CopyOrigin& origin
                        = reduced_copy_origins_[node.item_type_id][reduced_copy_index];
                    solution_builder.set_last_node_item(origin.item_type_id);
                }
            }
        }
    }

    return solution_builder.build();
}
