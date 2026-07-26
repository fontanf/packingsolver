#include "rectangle/benders_decomposition.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/dual_feasible_functions.hpp"

#include "onedimensional/milp_assignment.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/**
 * A cut (dual-feasible-function cut, no-good cut, or pairwise-
 * incompatibility cut) turned into a onedimensional resource on a specific
 * bin type.
 *
 * A per-item-type consumption is a per-copy schedule (see
 * 'onedimensional::BinType::item_resource_consumptions'), not a single
 * scalar: a dual-feasible-function cut is a plain linear inequality on item
 * counts, so a uniform (length-1) schedule is exact for it; a no-good cut
 * or pairwise-incompatibility cut instead needs "at least N copies of this
 * item type", which a uniform per-unit consumption cannot express (it would
 * only cap the *combined* total of the item types involved, wrongly
 * excluding unrelated combinations using more of one item type and none of
 * another) - achieved exactly via 'threshold_schedule' below.
 */
struct ResourceCut
{
    double capacity;
    std::vector<std::pair<ItemTypeId, std::vector<double>>> consumption;
};

/**
 * Per-copy consumption schedule making an item type's contribution to a
 * resource's total equal to 'min(count, threshold)': 'threshold' ones
 * followed by a single trailing zero. Contribution keeps growing only while
 * count < threshold, and never grows past it - so summing this schedule's
 * contribution over every item type in a selection, and setting the
 * resource's capacity to '(sum of thresholds) - 1', forbids exactly the
 * selection (and any selection using at least as many copies of every item
 * type in it), without excluding anything else.
 */
std::vector<double> threshold_schedule(ItemPos threshold)
{
    std::vector<double> schedule(threshold, 1.0);
    schedule.push_back(0.0);
    return schedule;
}

/**
 * 'true' iff two item types (possibly the same one twice) cannot both be
 * packed together in a single bin of the given type: a direct
 * generalization of the single-item "self-incompatible" check (substitute
 * 'item_type_2 := item_type_1' below and it reduces exactly to it, since
 * '2 * x > bin_x' follows from 'x > bin_x / 2' plus 'x > bin_x / 2').
 */
bool items_incompatible(
        const ItemType& item_type_1,
        const ItemType& item_type_2,
        const BinType& bin_type)
{
    return (!item_type_1.oriented
                && !item_type_2.oriented
                && item_type_1.rect.min() + item_type_2.rect.min() > bin_type.rect.max())
        || (item_type_1.oriented
                && item_type_2.oriented
                && item_type_1.rect.x + item_type_2.rect.x > bin_type.rect.x
                && item_type_1.rect.y + item_type_2.rect.y > bin_type.rect.y)
        || (!item_type_1.oriented
                && item_type_2.oriented
                && item_type_1.rect.max() + item_type_2.rect.x > bin_type.rect.x
                && item_type_1.rect.max() + item_type_2.rect.y > bin_type.rect.y)
        || (item_type_1.oriented
                && !item_type_2.oriented
                && item_type_1.rect.x + item_type_2.rect.max() > bin_type.rect.x
                && item_type_1.rect.y + item_type_2.rect.max() > bin_type.rect.y);
}

/**
 * 'true' iff item_type_2, in some orientation it is allowed to use, fits
 * within the footprint of item_type_1, in *every* orientation item_type_1
 * is allowed to use - i.e. wherever item_type_1 could validly be placed,
 * item_type_2 could be placed there too. Area alone is not sufficient for
 * this (a smaller-area item can still fail to fit a footprint a
 * larger-area item fits, e.g. a 4x2 item, area 8, cannot fit anywhere a
 * 1x10 item, area 10, fits, since neither of the 4x2 item's sides is
 * <= 1) - this checks actual width/height containment instead, across
 * every combination of the two item types' allowed rotations.
 */
bool item_type_fits_footprint_of(
        const ItemType& item_type_2,
        const ItemType& item_type_1)
{
    std::vector<std::pair<Length, Length>> footprints_1;
    footprints_1.push_back({item_type_1.rect.x, item_type_1.rect.y});
    if (!item_type_1.oriented)
        footprints_1.push_back({item_type_1.rect.y, item_type_1.rect.x});

    for (const std::pair<Length, Length>& footprint: footprints_1) {
        bool fits = (item_type_2.rect.x <= footprint.first
                    && item_type_2.rect.y <= footprint.second)
                || (!item_type_2.oriented
                    && item_type_2.rect.y <= footprint.first
                    && item_type_2.rect.x <= footprint.second);
        if (!fits)
            return false;
    }
    return true;
}

/**
 * Per-bin-type resources capturing geometric information the onedimensional
 * master (which only knows about item/bin area) cannot see on its own.
 * Computed once, upfront: these do not depend on any particular Benders
 * iteration.
 *
 * Item types that fit a bin type by area but not by 2D dimension (e.g. a
 * 1x100 item in a 10x10 bin) are handled separately, as onedimensional
 * eligibility rather than a resource here - see 'build_master_instance':
 * eligibility excludes the (item type, bin type) pair from ever getting a
 * MILP variable at all, which is strictly cheaper than creating one and
 * then forcing it to zero with a resource row.
 */
struct StaticBinTypeResources
{
    /**
     * Cuts forbidding pairs of item types that cannot coexist in a single
     * bin instance of this type ('i == j' means at most one copy of that
     * type fits alone).
     */
    std::vector<ResourceCut> incompatible_pair_cuts;
};

std::vector<StaticBinTypeResources> compute_static_bin_type_resources(
        const Instance& instance)
{
    std::vector<StaticBinTypeResources> resources(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        StaticBinTypeResources& bin_type_resources = resources[bin_type_id];
        for (ItemTypeId item_type_id_1 = 0;
                item_type_id_1 < instance.number_of_item_types();
                ++item_type_id_1) {
            const ItemType& item_type_1 = instance.item_type(item_type_id_1);
            if (!instance.item_type_fits_bin_type(item_type_id_1, bin_type_id))
                continue;
            for (ItemTypeId item_type_id_2 = item_type_id_1;
                    item_type_id_2 < instance.number_of_item_types();
                    ++item_type_id_2) {
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                if (!instance.item_type_fits_bin_type(item_type_id_2, bin_type_id))
                    continue;
                if (!items_incompatible(item_type_1, item_type_2, bin_type))
                    continue;
                ResourceCut cut;
                if (item_type_id_2 == item_type_id_1) {
                    cut.capacity = 1.0;
                    cut.consumption.push_back({item_type_id_1, threshold_schedule(2)});
                } else {
                    cut.capacity = 1.0;
                    cut.consumption.push_back({item_type_id_1, threshold_schedule(1)});
                    cut.consumption.push_back({item_type_id_2, threshold_schedule(1)});
                }
                bin_type_resources.incompatible_pair_cuts.push_back(cut);
            }
        }
    }
    return resources;
}

/**
 * Upper bound, for each bin type, on the number of bin instances of that
 * type any solution could ever use: the minimum number of bins of that
 * type needed to pack, via true 2D geometric packing (not just an area
 * check - the master itself only sees area, but this bound must remain
 * valid regardless of what the master sees), every item type that fits
 * it, using only bins of that type.
 *
 * This is sound and stays valid forever, independent of any resource cut
 * added later: a cut only ever excludes packings already proven
 * geometrically infeasible, so the relaxed (onedimensional, area + cuts)
 * master can never need more bin instances of a type than the true
 * geometric problem itself would - any solution using a subset of these
 * items needs at most as many bins as packing all of them does. This is
 * what makes it safe to compute once, upfront, rather than re-estimating
 * it every iteration from the (increasingly cut-constrained) master
 * instance: unlike an estimate derived from the current area-only
 * relaxation, which would need to grow as cuts tighten it, a bound
 * derived from true geometry already dominates whatever any amount of
 * (sound) cuts could ever require.
 *
 * Only meaningful for the 'VariableSizedBinPacking' objective, the only
 * one where the number of bin instances of a type is a decision rather
 * than fixed by the instance; for every other objective, the instance's
 * own bin type copies are already exact and are returned unchanged. Not
 * called at all for 'Knapsack' (see the call site): its master already
 * uses the instance's own bin type copies directly, so computing this
 * would be pure wasted work.
 */
std::vector<BinPos> compute_bin_type_upper_bounds(
        const Instance& instance,
        const BendersDecompositionParameters& parameters)
{
    std::vector<BinPos> bin_type_upper_bounds(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (instance.objective() != Objective::VariableSizedBinPacking) {
            bin_type_upper_bounds[bin_type_id] = bin_type.copies;
            continue;
        }

        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::BinPacking);
        sub_instance_builder.set_parameters(instance.parameters());
        BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
        sub_instance_builder.set_bin_type_copies(sub_bin_type_id, -1);
        sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);

        bool has_fitting_item = false;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id))
                continue;
            sub_instance_builder.add_item_type(instance, item_type_id);
            has_fitting_item = true;
        }
        if (!has_fitting_item) {
            bin_type_upper_bounds[bin_type_id] = 0;
            continue;
        }
        Instance sub_instance = sub_instance_builder.build();

        OptimizeParameters sub_parameters;
        sub_parameters.verbosity_level = 0;
        sub_parameters.timer = parameters.timer;
        sub_parameters.optimization_mode = OptimizationMode::NotAnytimeDeterministic;
        sub_parameters.use_tree_search = true;
        sub_parameters.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
        auto sub_output = optimize(sub_instance, sub_parameters);

        BinPos bin_type_upper_bound = sub_output.solution_pool.best().number_of_bins();
        if (bin_type.copies != -1)
            bin_type_upper_bound = std::min(bin_type_upper_bound, bin_type.copies);
        bin_type_upper_bounds[bin_type_id] = bin_type_upper_bound;
    }
    return bin_type_upper_bounds;
}

/**
 * Item type precedence pairs (dominated_item_type_id, dominating_item_type_id)
 * to declare on the onedimensional master via
 * 'InstanceBuilder::add_item_type_precedence' (see its doc comment, and
 * 'onedimensional::milp_assignment''s "Constraints: item type precedence",
 * for the exact semantics this expresses and why it is sound - and, unlike
 * the pairwise-incompatibility/no-good-cut resources above, correctly
 * multi-bin-safe). Computed once, upfront: these do not depend on any
 * particular Benders iteration.
 *
 * item_type_2 dominates item_type_1 when profit_2 >= profit_1 and
 * item_type_2 fits the footprint of item_type_1 (see
 * 'item_type_fits_footprint_of'; ties broken by lower id, to avoid a
 * mutual-domination deadlock), *and* item_type_2 is eligible for every bin
 * type item_type_1 is eligible for (a superset, not just "both fit
 * somewhere"): the underlying exchange argument (swap one dominated unit
 * for one dominating unit, which always fits in the same freed space, and
 * never decreases profit) needs to work wherever item_type_1 might
 * actually be placed, so item_type_2 must be placeable there too. Not
 * declared at all for non-Knapsack objectives: 'milp_assignment' makes any
 * declared pair an automatic no-op for those (their demand constraints
 * already force every item type's count, so there is never a "free" copy
 * of the dominating item type to swap in), so skipping them here just
 * avoids the wasted MILP size.
 */
std::vector<std::pair<ItemTypeId, ItemTypeId>> compute_item_type_precedences(
        const Instance& instance)
{
    std::vector<std::pair<ItemTypeId, ItemTypeId>> precedences;
    if (instance.objective() != Objective::Knapsack)
        return precedences;
    for (ItemTypeId item_type_id_1 = 0;
            item_type_id_1 < instance.number_of_item_types();
            ++item_type_id_1) {
        const ItemType& item_type_1 = instance.item_type(item_type_id_1);
        for (ItemTypeId item_type_id_2 = 0;
                item_type_id_2 < instance.number_of_item_types();
                ++item_type_id_2) {
            if (item_type_id_2 == item_type_id_1)
                continue;
            const ItemType& item_type_2 = instance.item_type(item_type_id_2);
            if (item_type_2.profit < item_type_1.profit)
                continue;
            if (!item_type_fits_footprint_of(item_type_2, item_type_1))
                continue;
            bool fully_tied = item_type_2.profit == item_type_1.profit
                    && item_type_fits_footprint_of(item_type_1, item_type_2);
            if (fully_tied && item_type_id_2 >= item_type_id_1)
                continue;
            bool item_1_fits_somewhere = false;
            bool superset = true;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types();
                    ++bin_type_id) {
                if (!instance.item_type_fits_bin_type(item_type_id_1, bin_type_id))
                    continue;
                item_1_fits_somewhere = true;
                if (!instance.item_type_fits_bin_type(item_type_id_2, bin_type_id)) {
                    superset = false;
                    break;
                }
            }
            if (!item_1_fits_somewhere || !superset)
                continue;
            precedences.push_back({item_type_id_1, item_type_id_2});
        }
    }
    return precedences;
}

/** Add a cut as a new resource on a bin type. */
void add_cut_as_resource(
        onedimensional::InstanceBuilder& master_instance_builder,
        BinTypeId master_bin_type_id,
        const ResourceCut& cut)
{
    ResourceId resource_id = master_instance_builder.add_bin_type_resource(
            master_bin_type_id, cut.capacity);
    for (const std::pair<ItemTypeId, std::vector<double>>& entry: cut.consumption) {
        for (ItemPos item_copy = 0;
                item_copy < (ItemPos)entry.second.size();
                ++item_copy) {
            master_instance_builder.add_resource_consumption(
                    master_bin_type_id,
                    resource_id,
                    entry.first,
                    item_copy,
                    entry.second[item_copy]);
        }
    }
}

/** Build the onedimensional master instance for the current iteration. */
onedimensional::Instance build_master_instance(
        const Instance& instance,
        const std::vector<StaticBinTypeResources>& static_resources,
        const std::vector<std::pair<ItemTypeId, ItemTypeId>>& item_type_precedences,
        const std::vector<BinPos>& bin_type_upper_bounds,
        const std::vector<std::vector<ResourceCut>>& dff_cuts_by_bin_type,
        const std::vector<std::vector<ResourceCut>>& no_good_cuts_by_bin_type)
{
    onedimensional::InstanceBuilder master_instance_builder;
    master_instance_builder.set_objective(instance.objective());

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeId master_bin_type_id = master_instance_builder.add_bin_type(bin_type.area());
        master_instance_builder.set_bin_type_cost(master_bin_type_id, bin_type.cost);
        master_instance_builder.set_bin_type_copies(
                master_bin_type_id,
                (instance.objective() == Objective::Knapsack)?
                    bin_type.copies:
                    bin_type_upper_bounds[bin_type_id]);
        master_instance_builder.set_bin_type_copies_min(master_bin_type_id, bin_type.copies_min);
        // The original bin type's eligibility ids are not copied here: every
        // item type that needs any eligibility restriction (the original
        // one, a 2D-dimension-fit restriction, or both) gets its own fresh
        // eligibility id below instead, so the original id values are never
        // referenced in the master instance.

        const StaticBinTypeResources& bin_type_resources = static_resources[bin_type_id];
        for (const ResourceCut& cut: bin_type_resources.incompatible_pair_cuts) {
            add_cut_as_resource(master_instance_builder, master_bin_type_id, cut);
        }
        for (const ResourceCut& cut: dff_cuts_by_bin_type[bin_type_id]) {
            add_cut_as_resource(master_instance_builder, master_bin_type_id, cut);
        }
        for (const ResourceCut& cut: no_good_cuts_by_bin_type[bin_type_id]) {
            add_cut_as_resource(master_instance_builder, master_bin_type_id, cut);
        }
    }

    // Fresh eligibility ids, local to this master instance, one per item
    // type that needs any restriction; unrestricted below.
    EligibilityId next_eligibility_id = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId master_item_type_id = master_instance_builder.add_item_type(item_type.area());
        master_instance_builder.set_item_type_copies(master_item_type_id, item_type.copies);
        if (instance.objective() == Objective::Knapsack) {
            master_instance_builder.set_item_type_profit(master_item_type_id, item_type.profit);
        }

        bool fits_all_bins = true;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (!instance.item_type_fits_bin_type(item_type_id, bin_type_id)) {
                fits_all_bins = false;
                break;
            }
        }
        if (fits_all_bins) {
            // No restriction at all: leave the default eligibility_id (-1).
            continue;
        }

        EligibilityId master_eligibility_id = next_eligibility_id++;
        master_instance_builder.set_item_type_eligibility(master_item_type_id, master_eligibility_id);
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (instance.item_type_fits_bin_type(item_type_id, bin_type_id))
                master_instance_builder.add_bin_type_eligibility(bin_type_id, master_eligibility_id);
        }
    }

    // Bin-type and item-type ids in the master are numerically identical to
    // the original instance's (every type is always added, never a
    // subset), so the precedence pairs can be declared directly, unchanged.
    for (const std::pair<ItemTypeId, ItemTypeId>& precedence: item_type_precedences) {
        master_instance_builder.add_item_type_precedence(
                precedence.first, precedence.second);
    }

    return master_instance_builder.build();
}

/**
 * Aggregate a onedimensional solution bin's flat (one entry per unit)
 * item list into (item_type_id, count) pairs.
 */
std::vector<std::pair<ItemTypeId, ItemPos>> aggregate_bin_items(
        const onedimensional::SolutionBin& bin,
        ItemTypeId number_of_item_types)
{
    std::vector<ItemPos> counts(number_of_item_types, 0);
    for (const onedimensional::SolutionItem& item: bin.items)
        counts[item.item_type_id]++;
    std::vector<std::pair<ItemTypeId, ItemPos>> result;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types;
            ++item_type_id) {
        if (counts[item_type_id] > 0)
            result.push_back({item_type_id, counts[item_type_id]});
    }
    return result;
}

}

BendersDecompositionOutput packingsolver::rectangle::benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters)
{
    BendersDecompositionOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    std::vector<StaticBinTypeResources> static_resources = compute_static_bin_type_resources(instance);
    std::vector<std::pair<ItemTypeId, ItemTypeId>> item_type_precedences = compute_item_type_precedences(instance);
    // Not needed for 'Knapsack': 'build_master_instance' uses the instance's
    // own bin type copies directly for it (see there).
    std::vector<BinPos> bin_type_upper_bounds;
    if (instance.objective() != Objective::Knapsack) {
        bin_type_upper_bounds = compute_bin_type_upper_bounds(instance, parameters);

        // Check end.
        if (parameters.timer.needs_to_end()) {
            algorithm_formatter.end();
            return output;
        }
    }

    // Cuts accumulated so far, bucketed per bin type.
    std::vector<std::vector<ResourceCut>> dff_cuts_by_bin_type(instance.number_of_bin_types());
    std::vector<std::vector<ResourceCut>> no_good_cuts_by_bin_type(instance.number_of_bin_types());

    // 'true' iff every cut added so far is backed by a proof of
    // infeasibility (a dual-feasible-function cut always is; a no-good cut
    // only is if its subproblem was proven infeasible, not merely
    // incomplete). The master's relaxation bound is only a valid bound as
    // long as this holds: a cut added without proof may have removed a
    // genuinely feasible (and possibly optimal) region.
    bool all_cuts_proven_infeasible = true;

    for (output.number_of_iterations = 0;
            ;
            ++output.number_of_iterations) {

        // Check maximum number of iterations.
        if (parameters.maximum_number_of_iterations >= 0
                && output.number_of_iterations >= parameters.maximum_number_of_iterations) {
            break;
        }

        // Build and solve the master problem.
        onedimensional::Instance master_instance = build_master_instance(
                instance, static_resources, item_type_precedences, bin_type_upper_bounds,
                dff_cuts_by_bin_type, no_good_cuts_by_bin_type);
        onedimensional::MilpAssignmentParameters master_parameters;
        master_parameters.solver = parameters.solver;
        master_parameters.verbosity_level = 0;
        master_parameters.timer = parameters.timer;
        onedimensional::MilpAssignmentOutput master_output = onedimensional::milp_assignment(
                master_instance, master_parameters);

        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        if (all_cuts_proven_infeasible) {
            if (instance.objective() == Objective::Knapsack) {
                algorithm_formatter.update_knapsack_bound(master_output.knapsack_bound);
            } else if (instance.objective() == Objective::BinPacking) {
                algorithm_formatter.update_bin_packing_bound(master_output.bin_packing_bound);
            } else if (instance.objective() == Objective::VariableSizedBinPacking) {
                algorithm_formatter.update_variable_sized_bin_packing_bound(
                        master_output.variable_sized_bin_packing_bound);
            }
        }
        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        const onedimensional::Solution& master_solution = master_output.solution_pool.best();

        // For 'Knapsack', the master's demand constraint is '<=', so the
        // empty selection is always feasible: the master can never itself
        // be infeasible. For 'BinPacking' and 'VariableSizedBinPacking',
        // the demand constraint is an equality (every item must be
        // packed), so a master with too little bin supply genuinely can be
        // infeasible - in which case 'milp_assignment' leaves its solution
        // pool at its default empty entry (0 bins), with no other signal.
        // Since the timer has already been checked above, an empty result
        // here is not a timeout: it means the (sub-)problem given to the
        // master has no feasible solution at all, which should not happen
        // for a well-formed instance.
        if (master_solution.number_of_bins() == 0
                && instance.number_of_items() > 0
                && instance.objective() != Objective::Knapsack) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "the master problem should not be infeasible.");
        }

        // Pass 1: check every bin of the master's candidate for a violated
        // dual-feasible-function inequality before paying for the
        // (potentially expensive) feasibility subproblems. Unlike a
        // no-good cut on an exact selection, a dual-feasible-function cut
        // is a general inequality valid for any selection assigned to a
        // bin of that type, so it is added as a permanent resource. If any
        // bin has one, add the most-violated cut for every bin that has
        // one, and skip the subproblems entirely this iteration.
        bool dff_violation_found = false;
        for (BinPos master_bin_pos = 0;
                master_bin_pos < master_solution.number_of_different_bins();
                ++master_bin_pos) {
            const onedimensional::SolutionBin& master_bin = master_solution.bin(master_bin_pos);
            std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = aggregate_bin_items(
                    master_bin, instance.number_of_item_types());
            DualFeasibleFunctionsCut dff_cut = find_most_violated_dual_feasible_function_cut(
                    instance, master_bin.bin_type_id, selected_items);
            if (dff_cut.found) {
                dff_violation_found = true;
                ResourceCut resource_cut;
                resource_cut.capacity = dff_cut.bound;
                for (ItemTypeId item_type_id = 0;
                        item_type_id < instance.number_of_item_types();
                        ++item_type_id) {
                    if (dff_cut.coefficients[item_type_id] != 0.0) {
                        resource_cut.consumption.push_back(
                                {item_type_id, {dff_cut.coefficients[item_type_id]}});
                    }
                }
                dff_cuts_by_bin_type[master_bin.bin_type_id].push_back(resource_cut);
            }
        }
        if (dff_violation_found)
            continue;

        // Pass 2: no bin had a dual-feasible-function violation; run the
        // actual geometric feasibility subproblem for every bin, adding one
        // no-good cut per infeasible bin found.
        Solution solution(instance);
        bool all_bins_feasible = true;
        bool need_to_end = false;
        for (BinPos master_bin_pos = 0;
                master_bin_pos < master_solution.number_of_different_bins();
                ++master_bin_pos) {
            const onedimensional::SolutionBin& master_bin = master_solution.bin(master_bin_pos);
            std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = aggregate_bin_items(
                    master_bin, instance.number_of_item_types());

            // Build subproblem instance.
            InstanceBuilder sub_instance_builder;
            sub_instance_builder.set_objective(Objective::Feasibility);
            sub_instance_builder.set_parameters(instance.parameters());
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(
                    instance, master_bin.bin_type_id);
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            std::vector<ItemTypeId> sub_to_orig;
            for (const std::pair<ItemTypeId, ItemPos>& p: selected_items) {
                ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(instance, p.first);
                sub_instance_builder.set_item_type_copies(sub_item_type_id, p.second);
                sub_to_orig.push_back(p.first);
            }
            Instance sub_instance = sub_instance_builder.build();

            // Solve.
            OptimizeParameters sub_parameters;
            sub_parameters.verbosity_level = 0;
            sub_parameters.timer = parameters.timer;
            sub_parameters.optimization_mode
                = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
                OptimizationMode::NotAnytimeSequential:
                OptimizationMode::NotAnytimeDeterministic;
            sub_parameters.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
            auto sub_output = optimize(sub_instance, sub_parameters);
            const Solution& sub_solution = sub_output.solution_pool.best();

            if (sub_solution.number_of_bins() > 0) {
                solution.append(
                        sub_solution,
                        0,  // bin_pos
                        1,  // copies
                        {master_bin.bin_type_id},
                        sub_to_orig);
            }

            // Check end.
            if (parameters.timer.needs_to_end()) {
                need_to_end = true;
                break;
            }

            if (!sub_solution.full()) {
                // Otherwise, add the corresponding no-good cut.
                all_bins_feasible = false;
                ResourceCut resource_cut;
                ItemPos cut_size = 0;
                for (const std::pair<ItemTypeId, ItemPos>& p: selected_items) {
                    cut_size += p.second;
                    resource_cut.consumption.push_back({p.first, threshold_schedule(p.second)});
                }
                resource_cut.capacity = (double)cut_size - 1;
                no_good_cuts_by_bin_type[master_bin.bin_type_id].push_back(resource_cut);
                // As long as every cut added so far is backed by a proof of
                // infeasibility, the master's feasible region has never
                // been shrunk beyond what the true problem allows, so the
                // current MILP relaxation bound remains a valid bound.
                all_cuts_proven_infeasible
                    = all_cuts_proven_infeasible && sub_output.is_proven_infeasible;
            }
        }

        // Update solution.
        if (solution.number_of_bins() > 0) {
            std::stringstream ss;
            ss << "BD it " << output.number_of_iterations;
            algorithm_formatter.update_solution(solution, ss.str());
        }

        if (need_to_end)
            break;

        if (all_bins_feasible) {
            // Every bin of the master's candidate is geometrically
            // feasible: it is a complete, valid, optimal (given the cuts
            // added so far) solution. Stop.
            break;
        }
    }

    algorithm_formatter.end();
    return output;
}
