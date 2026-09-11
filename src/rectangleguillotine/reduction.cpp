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
                original_item_type.rect.w,
                original_item_type.rect.h,
                original_item_type.oriented,
                original_item_type.stack_id);
        instance_builder.set_item_type_profit(new_item_type_id, original_item_type.profit);
        instance_builder.set_item_type_copies(new_item_type_id, total_copies);
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
            solution_builder.add_bin(
                    solution_bin.bin_type_id,
                    batch,
                    solution_bin.first_cut_orientation);
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
                    ItemTypeId original_item_type_id = (bin_fits_in_current_runs)?
                        remaining_runs[node.item_type_id].back().item_type_id:
                        consume_one_origin(remaining_runs[node.item_type_id]);
                    solution_builder.set_last_node_item(original_item_type_id);
                }
            }

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
