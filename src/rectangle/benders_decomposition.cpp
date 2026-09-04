#include "rectangle/benders_decomposition.hpp"

#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/dual_feasible_functions.hpp"
#include "rectangle/bar_relaxation.hpp"

#include "onedimensional/milp_assignment.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"

#include <algorithm>

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
 * 'onedimensional::Resource::item_consumptions'), not a single
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
 * Lift a no-good cut via the sequential lifting procedure of Balas (1975)
 * and Wolsey (1975), as adapted by Côté, Haouari & Iori (2021), Section 7.3
 * ("Lifting the Cut", their Algorithm 2): starting from 'S := C' (the
 * original cut's item types, each worth profit 1 per copy up to its
 * threshold), visit every item type not already in the cut, in id order,
 * and compute how many "covering units" it is worth by solving a 2D
 * knapsack that forces one copy of it into the bin alongside the best
 * achievable selection from 'S'. The paper solves that knapsack exactly
 * (2D-KP); this uses the bar relaxation ('bar_relaxation.hpp', Scheithauer
 * 1999) instead, the same substitution the paper itself makes for the same
 * reason (2D-KP is NP-hard, the bar relaxation is not) via its "2D-UKP".
 *
 * Concretely, for candidate j*: let 'm' be the running total of 'S' (sum of
 * each entry's threshold, i.e. the original cut's capacity + 1, plus every
 * previously lifted item's own coefficient). Solve a Knapsack instance on
 * this bin type with every item type in 'S' at its tracked (copies,
 * profit) - profit 1 per copy for the original cut's items, or a lifted
 * item's own coefficient for exactly 1 copy (see below) - plus j* forced
 * into exactly 1 copy via 'copies_min' (a hard bound, not a profit
 * incentive: bar_relaxation is only a linear relaxation, so a sufficiently
 * high profit only guarantees j* wins the *aggregate* trade-off against
 * everything 'S' could contribute in total - it says nothing about the
 * *marginal*, per-unit-of-shared-capacity trade-off the LP actually
 * optimizes over, which a single very dense item already in 'S', e.g. a
 * previously-lifted one with a large coefficient on a small footprint,
 * could still win, leaving j* selected fractionally below 1 - so
 * 'copies_min' is the only mechanism that actually guarantees j*'s
 * quantity reaches 1 at the optimum), at profit 0 (j*'s own real profit is
 * irrelevant here - only 'S''s achievable profit alongside its forced
 * presence matters). The reported bound is then exactly 'S''s contribution
 * alongside that forced copy - a valid (since the bar relaxation is itself
 * a relaxation) upper bound on the exact 2D-KP value the paper calls for.
 * The lift coefficient is then 'alpha = max(0, m - 1 - that bound)',
 * floored to the nearest integer below it (never above: the true,
 * exact-2D-KP-based coefficient is itself an integer at least as large as
 * any valid upper-bound-based estimate of it, so rounding down can only
 * make this estimate more conservative, never unsound). If alpha > 0, j*
 * joins 'S' - contributing exactly 1 copy at profit 'alpha' to every
 * future candidate's knapsack (not 'alpha' copies at profit 1: unlike the
 * original cut's items, which really do represent that many physical
 * units, j* is a single physical item whose *equivalent covering worth*
 * happens to be 'alpha') - and is appended to the same, single cut being
 * built, with its own row contribution capped at 'alpha' via
 * 'threshold_schedule' exactly as for the original items.
 *
 * Unlike a purely geometric domination check (fits within the footprint of
 * some subset of 'S', therefore forces at least that much room), this
 * needs no notion of "dominates": the relaxation bound already accounts
 * for every item type in 'S' at once, so nothing here is unsound the way
 * merging several independently-computed dominators into one shared row
 * would be. It also generalizes cleanly to non-geometric infeasibility
 * (e.g. a weight or axle-load limit the original cut's infeasibility proof
 * actually turned on): the bar relaxation is a valid upper bound on the
 * exact 2D knapsack value regardless of which other constraints made 'C'
 * infeasible in the first place, since dropping constraints from a
 * relaxation can only raise that bound, never lower it below the truth.
 */
ResourceCut lift_no_good_cut(
        const ResourceCut& original_cut,
        const Instance& instance,
        BinTypeId bin_type_id,
        const BendersDecompositionParameters& parameters)
{
    //std::cout << "lift_no_good_cut" << std::endl;
    struct SEntry
    {
        ItemTypeId item_type_id;
        ItemPos copies;
        double profit;
    };

    std::vector<bool> in_cut(instance.number_of_item_types(), false);
    std::vector<SEntry> s_entries;
    double m = original_cut.capacity + 1.0;
    for (const std::pair<ItemTypeId, std::vector<double>>& entry: original_cut.consumption) {
        ItemPos threshold = (ItemPos)entry.second.size() - 1;
        s_entries.push_back({entry.first, threshold, 1.0});
        in_cut[entry.first] = true;
    }

    ResourceCut lifted_cut = original_cut;
    for (ItemTypeId candidate_id = 0;
            candidate_id < instance.number_of_item_types();
            ++candidate_id) {
        if (in_cut[candidate_id])
            continue;
        if (!instance.item_type_fits_bin_type(candidate_id, bin_type_id))
            continue;
        if (parameters.timer.needs_to_end())
            break;

        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Knapsack);
        sub_instance_builder.set_parameters(instance.parameters());
        BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
        sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
        sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
        for (const SEntry& s_entry: s_entries) {
            ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(instance, s_entry.item_type_id);
            sub_instance_builder.set_item_type_profit(sub_item_type_id, s_entry.profit);
            sub_instance_builder.set_item_type_copies(sub_item_type_id, s_entry.copies);
        }
        // j* forced into exactly 1 copy via 'copies_min', at profit 0 (see
        // the function-level comment above for why a profit incentive
        // cannot substitute for this).
        ItemTypeId sub_candidate_id = sub_instance_builder.add_item_type(instance, candidate_id);
        sub_instance_builder.set_item_type_profit(sub_candidate_id, 0.0);
        sub_instance_builder.set_item_type_copies(sub_candidate_id, 1);
        sub_instance_builder.set_item_type_copies_min(sub_candidate_id, 1);
        Instance sub_instance = sub_instance_builder.build();

        BarRelaxationParameters bar_relaxation_parameters;
        bar_relaxation_parameters.verbosity_level = 0;
        bar_relaxation_parameters.timer = parameters.timer;
        BarRelaxationOutput bar_relaxation_output = bar_relaxation(sub_instance, bar_relaxation_parameters);

        // j* contributes 0 to the objective by construction, so the
        // reported bound is exactly 'S''s contribution alongside it -
        // no arithmetic needed to back a forced profit back out.
        double s_contribution = bar_relaxation_output.knapsack_bound;
        double alpha = m - 1.0 - s_contribution;
        // '+ 1e-6' guards against floating-point noise from the LP solve
        // rounding an exact integer value down (e.g. 3 computed as
        // 2.9999999997): never enough to round a genuinely fractional
        // value up to the next integer, which would overstate alpha and
        // make the cut unsound.
        ItemPos alpha_rounded = (alpha > 0.0)?
            (ItemPos)std::floor(alpha + 1e-6) : 0;

        if (alpha_rounded > 0) {
            lifted_cut.consumption.push_back(
                    {candidate_id, threshold_schedule(alpha_rounded)});
            s_entries.push_back({candidate_id, 1, (double)alpha_rounded});
            m += alpha_rounded;
        }
    }
    //std::cout << "lift_no_good_cut end" << std::endl;
    return lifted_cut;
}

/**
 * Result of checking whether a selection of items fits together in a
 * single bin of a given type: 'Unknown' means the feasibility subproblem
 * neither found a complete packing nor exhausted the search space within
 * its budget (see 'BendersDecompositionParameters::subproblem_queue_size')
 * - i.e. it is genuinely inconclusive, not evidence of infeasibility.
 */
enum class SelectionFeasibility
{
    Feasible,
    ProvenInfeasible,
    Unknown,
};

/**
 * Check whether 'selected_items' (copies of item types, possibly zero for
 * some) all fit together in a single bin of the given type; see
 * 'SelectionFeasibility'.
 */
SelectionFeasibility selection_feasibility(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        const BendersDecompositionParameters& parameters)
{
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Feasibility);
    sub_instance_builder.set_parameters(instance.parameters());
    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
    sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
    bool any_items = false;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items) {
        if (p.second <= 0)
            continue;
        any_items = true;
        ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(instance, p.first);
        sub_instance_builder.set_item_type_copies(sub_item_type_id, p.second);
    }
    if (!any_items)
        return SelectionFeasibility::Feasible;
    Instance sub_instance = sub_instance_builder.build();

    OptimizeParameters sub_parameters;
    sub_parameters.verbosity_level = 0;
    sub_parameters.timer = parameters.timer;
    sub_parameters.optimization_mode
        = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
        OptimizationMode::NotAnytimeSequential:
        OptimizationMode::NotAnytimeDeterministic;
    sub_parameters.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
    auto sub_output = optimize(sub_instance, sub_parameters);
    if (sub_output.solution_pool.best().feasible())
        return SelectionFeasibility::Feasible;
    if (sub_output.is_proven_infeasible)
        return SelectionFeasibility::ProvenInfeasible;
    return SelectionFeasibility::Unknown;
}

/**
 * Aggregate a list of item units (flattened, one entry per copy) into
 * (item_type_id, count) pairs, skipping any unit marked 'removed'.
 */
std::vector<std::pair<ItemTypeId, ItemPos>> aggregate_units(
        const Instance& instance,
        const std::vector<ItemTypeId>& units,
        const std::vector<bool>& removed)
{
    std::vector<ItemPos> counts(instance.number_of_item_types(), 0);
    for (size_t unit_pos = 0; unit_pos < units.size(); ++unit_pos) {
        if (!removed[unit_pos])
            counts[units[unit_pos]]++;
    }
    std::vector<std::pair<ItemTypeId, ItemPos>> result;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (counts[item_type_id] > 0)
            result.push_back({item_type_id, counts[item_type_id]});
    }
    return result;
}

/**
 * A minimal infeasible subset found by 'enumerate_minimal_infeasible_subsets'
 * below, together with whether every geometric feasibility check along its
 * derivation (from the original selection down to this exact subset) was a
 * genuine proof of infeasibility ('SelectionFeasibility::ProvenInfeasible')
 * rather than merely inconclusive ('SelectionFeasibility::Unknown', treated
 * as infeasible too - see 'enumerate_minimal_infeasible_subsets_dfs' - so
 * that the search still gets to shrink the master's feasible region on the
 * strength of a good guess, not just on hard proof). 'proven' is what
 * decides whether the cut built from 'items' may be trusted for bound
 * purposes at the call site (see 'all_cuts_proven_infeasible').
 */
struct MinimalInfeasibleSubset
{
    std::vector<std::pair<ItemTypeId, ItemPos>> items;
    bool proven;
};

/**
 * DFS step of 'enumerate_minimal_infeasible_subsets' below.
 *
 * Try removing, one at a time, every unit from 'next_unit_pos' onwards -
 * only ever considering units at or past the position of the last unit
 * removed to reach this node (the standard way to enumerate every subset
 * along exactly one path, without generating the same one twice).
 * Recursing into any resulting selection that is not 'Feasible' (see
 * 'SelectionFeasibility') - both 'ProvenInfeasible' and 'Unknown' count,
 * since an inconclusive subproblem is still worth speculatively treating
 * as infeasible for the sake of finding a smaller (stronger) cut, it just
 * taints 'path_proven' from that point on. If no removal was found
 * infeasible (including if there is nothing left to try removing), the
 * current selection - as 'removed' stands on entry to this call - can be
 * reduced no further along any explored branch, so it is itself a minimal
 * infeasible subset, recorded as a new cut with 'path_proven' as it
 * stands on entry (accumulated from the original selection's own proof
 * status down through every reduction step taken to reach here).
 */
void enumerate_minimal_infeasible_subsets_dfs(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& units,
        std::vector<bool>& removed,
        size_t next_unit_pos,
        bool path_proven,
        const BendersDecompositionParameters& parameters,
        Counter maximum_number_of_cuts,
        std::vector<MinimalInfeasibleSubset>& cuts)
{
    bool any_child_infeasible = false;
    for (size_t unit_pos = next_unit_pos;
            unit_pos < units.size() && (Counter)cuts.size() < maximum_number_of_cuts;
            ++unit_pos) {
        removed[unit_pos] = true;
        std::vector<std::pair<ItemTypeId, ItemPos>> child_selection =
            aggregate_units(instance, units, removed);
        SelectionFeasibility child_feasibility = selection_feasibility(
                instance, bin_type_id, child_selection, parameters);
        if (child_feasibility != SelectionFeasibility::Feasible) {
            any_child_infeasible = true;
            enumerate_minimal_infeasible_subsets_dfs(
                    instance, bin_type_id, units, removed, unit_pos + 1,
                    path_proven && (child_feasibility == SelectionFeasibility::ProvenInfeasible),
                    parameters, maximum_number_of_cuts, cuts);
        }
        removed[unit_pos] = false;
    }

    if (!any_child_infeasible && (Counter)cuts.size() < maximum_number_of_cuts)
        cuts.push_back({aggregate_units(instance, units, removed), path_proven});
}

/**
 * Enumerate up to 'maximum_number_of_cuts' minimal infeasible subsets of a
 * selection (items that do not all fit in a single bin of the given type),
 * via the DFS in 'enumerate_minimal_infeasible_subsets_dfs'. Each one is a
 * strictly stronger no-good cut than the original selection itself would
 * be: forbidding a subset forbids every combination that contains it,
 * which is strictly more than forbidding only the original (larger)
 * selection. Unlike the domination-based lift above, this needs no
 * exchange argument at all - it reuses the exact same geometric
 * feasibility check that established the original selection's own
 * infeasibility (proven or not, see 'selection_proven_infeasible' and
 * 'MinimalInfeasibleSubset') in the first place, just on fewer items each
 * time.
 *
 * Items are flattened into individual units and sorted by increasing area
 * first: trying to remove small items before large ones tends to converge
 * on minimal subsets built from the largest ("most essential") items,
 * which are usually the actual reason the selection cannot fit, rather
 * than on subsets that still include removable small padding.
 */
std::vector<MinimalInfeasibleSubset> enumerate_minimal_infeasible_subsets(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        bool selection_proven_infeasible,
        const BendersDecompositionParameters& parameters,
        Counter maximum_number_of_cuts)
{
    std::vector<ItemTypeId> units;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items) {
        for (ItemPos copy = 0; copy < p.second; ++copy)
            units.push_back(p.first);
    }
    std::stable_sort(
            units.begin(),
            units.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return instance.item_type(item_type_id_1).rect.area()
                    < instance.item_type(item_type_id_2).rect.area();
            });

    std::vector<bool> removed(units.size(), false);
    std::vector<MinimalInfeasibleSubset> cuts;
    enumerate_minimal_infeasible_subsets_dfs(
            instance, bin_type_id, units, removed, 0, selection_proven_infeasible,
            parameters, maximum_number_of_cuts, cuts);
    return cuts;
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
        master_instance_builder.set_bin_type_copies(master_bin_type_id, bin_type.copies);
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

        // Copy the original instance's own user-defined resources (as
        // opposed to the cuts above, which are also encoded as resources
        // but generated internally by this algorithm). Bin type and item
        // type ids are numerically identical between 'instance' and the
        // master (see the doc comment below), so no remapping is needed.
        // Resources are enforced directly by the master's own MILP this
        // way: the geometric slave subproblem never needs to know about
        // them at all, since the master never proposes a bin assignment
        // that violates one in the first place.
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            ResourceId master_resource_id = master_instance_builder.add_bin_type_resource(
                    master_bin_type_id,
                    resource.capacity,
                    resource.penalize,
                    resource.penalty);
            for (ItemTypeId item_type_id = 0;
                    item_type_id < (ItemTypeId)resource.item_consumptions.size();
                    ++item_type_id) {
                const std::vector<double>& schedule = resource.item_consumptions[item_type_id];
                for (ItemPos item_copy = 0;
                        item_copy < (ItemPos)schedule.size();
                        ++item_copy) {
                    master_instance_builder.add_resource_consumption(
                            master_bin_type_id,
                            master_resource_id,
                            item_type_id,
                            item_copy,
                            schedule[item_copy]);
                }
            }
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
        master_instance_builder.set_item_type_copies_min(master_item_type_id, item_type.copies_min);
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

/**
 * Total resource consumption a bin's item type counts (dense, indexed by
 * item_type_id) contribute to a cut, replaying its per-item-type,
 * per-copy consumption schedule exactly as
 * 'onedimensional::BinType::item_resource_consumption' does (see there):
 * cumulative, not just the marginal contribution of the last copy.
 */
double resource_cut_consumption(
        const ResourceCut& resource_cut,
        const std::vector<ItemPos>& item_type_counts)
{
    double consumption = 0.0;
    for (const std::pair<ItemTypeId, std::vector<double>>& entry: resource_cut.consumption) {
        ItemPos count = item_type_counts[entry.first];
        for (ItemPos copy = 0; copy < count; ++copy) {
            consumption += (copy < (ItemPos)entry.second.size())?
                entry.second[copy]:
                entry.second.back();
        }
    }
    return consumption;
}

/**
 * 'true' iff 'cut' (a resource on bin type 'bin_type_id') is violated by
 * some bin of that type in 'solution' - see 'resource_cut_consumption'.
 */
bool cut_violates_solution(
        const Instance& instance,
        const Solution& solution,
        BinTypeId bin_type_id,
        const ResourceCut& cut)
{
    for (BinPos solution_bin_pos = 0;
            solution_bin_pos < solution.number_of_different_bins();
            ++solution_bin_pos) {
        const SolutionBin& solution_bin = solution.bin(solution_bin_pos);
        if (solution_bin.bin_type_id != bin_type_id)
            continue;
        std::vector<ItemPos> item_type_counts(instance.number_of_item_types(), 0);
        for (const SolutionItem& item: solution_bin.items)
            item_type_counts[item.item_type_id]++;
        if (strictly_greater(resource_cut_consumption(cut, item_type_counts), cut.capacity))
            return true;
    }
    return false;
}

/**
 * Check that no accumulated cut (dual-feasible-function or no-good)
 * excludes 'solution' - the algorithm's own current best known feasible
 * solution to 'instance' itself, not the master's relaxation.
 *
 * Only meaningful once every no-good cut added so far is backed by a
 * genuine proof of infeasibility (dual-feasible-function cuts always
 * are; see 'all_cuts_proven_infeasible' at the call site): in that
 * regime, every cut is sound (derived from - and only from - a
 * combination proven infeasible in true 2D geometry), so the best known
 * solution, itself a genuinely feasible packing, must satisfy every one
 * of them. A violation here is therefore not a fluke: it is a proof that
 * some cut (or its lifting) is unsound.
 */
void check_cuts_against_best_solution(
        const Instance& instance,
        const Solution& solution,
        const std::vector<std::vector<ResourceCut>>& dff_cuts_by_bin_type,
        const std::vector<std::vector<ResourceCut>>& no_good_cuts_by_bin_type)
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        for (const ResourceCut& cut: dff_cuts_by_bin_type[bin_type_id]) {
            if (cut_violates_solution(instance, solution, bin_type_id, cut)) {
                throw std::logic_error(
                        FUNC_SIGNATURE + ": a dual-feasible-function cut excludes "
                        "the current best solution.");
            }
        }
        for (const ResourceCut& cut: no_good_cuts_by_bin_type[bin_type_id]) {
            if (cut_violates_solution(instance, solution, bin_type_id, cut)) {
                throw std::logic_error(
                        FUNC_SIGNATURE + ": a no-good cut excludes "
                        "the current best solution.");
            }
        }
    }
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

    // Number of iterations that actually solved at least one feasibility
    // subproblem so far - see
    // 'BendersDecompositionParameters::not_anytime_maximum_number_of_subproblem_solves'.
    Counter number_of_subproblem_solves = 0;

    for (output.number_of_iterations = 0;
            ;
            ++output.number_of_iterations) {
        //std::cout << "iteration " << output.number_of_iterations << std::endl;

        // Check maximum number of iterations.
        if (parameters.optimization_mode != OptimizationMode::Anytime
                && parameters.not_anytime_maximum_number_of_iterations >= 0
                && output.number_of_iterations >= parameters.not_anytime_maximum_number_of_iterations) {
            break;
        }

        // Build and solve the master problem.
        onedimensional::Instance master_instance = build_master_instance(
                instance, static_resources, item_type_precedences,
                dff_cuts_by_bin_type, no_good_cuts_by_bin_type);
        onedimensional::OptimizeParameters master_parameters;
        master_parameters.verbosity_level = 0;
        master_parameters.timer = parameters.timer;
        master_parameters.optimization_mode
            = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
            OptimizationMode::NotAnytimeSequential:
            OptimizationMode::NotAnytimeDeterministic;
        master_parameters.use_tree_search = parameters.master_problem_use_tree_search;
        master_parameters.use_milp_assignment = parameters.master_problem_use_milp_assignment;
        master_parameters.optimization_mode = OptimizationMode::Anytime;
        onedimensional::Output master_output = onedimensional::optimize(
                master_instance, master_parameters);

        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        if (all_cuts_proven_infeasible) {
            // 'master_output' is a 'onedimensional::Output', a different
            // type from this 'rectangle::AlgorithmFormatter''s own, so it
            // can't be forwarded via 'update_bounds' - checked manually
            // instead.
            if (master_output.is_proven_infeasible) {
                algorithm_formatter.update_is_proven_infeasible();
            } else if (instance.objective() == Objective::Knapsack) {
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
        if (dff_violation_found) {
            //std::cout << "dff_violation_found" << std::endl;
            continue;
        }

        // Check maximum number of subproblem solves. Checked here, not at
        // the top of the loop alongside
        // 'not_anytime_maximum_number_of_iterations', since only past this
        // point is it known that this iteration will actually solve at
        // least one subproblem below (the 'continue' above skips it
        // otherwise).
        if (parameters.optimization_mode != OptimizationMode::Anytime
                && parameters.not_anytime_maximum_number_of_subproblem_solves >= 0
                && number_of_subproblem_solves >= parameters.not_anytime_maximum_number_of_subproblem_solves) {
            break;
        }
        number_of_subproblem_solves++;

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
                // 'master_bin.copies' identical physical bins share this
                // exact item selection (that is what 'copies' > 1 on a
                // single master 'SolutionBin' entry means): since the
                // geometric check above only verifies one representative
                // instance (checking it 'master_bin.copies' times would be
                // redundant, they are all identical), every one of them
                // must be reported here, not just the one verified.
                solution.append_bin(
                        sub_solution,
                        0,  // bin_pos
                        master_bin.copies,
                        {master_bin.bin_type_id},
                        sub_to_orig);
            }

            // Check end.
            if (parameters.timer.needs_to_end()) {
                need_to_end = true;
                break;
            }

            if (!sub_solution.feasible()) {
                // Add a no-good cut for every minimal infeasible subset of
                // the selection found (see
                // 'enumerate_minimal_infeasible_subsets') - each one
                // strictly stronger than a single cut built from the whole
                // selection. Added regardless of whether the underlying
                // feasibility subproblem(s) actually *proved* infeasibility
                // or merely ran out of search budget without a definitive
                // answer either way (see 'sub_output.is_proven_infeasible',
                // 'SelectionFeasibility::Unknown'): an unproven cut may
                // still speculatively shrink the master's feasible region
                // and help it converge faster, at the cost of the region
                // possibly no longer being a strict superset of the true
                // one - which is exactly what 'MinimalInfeasibleSubset::proven'
                // (ANDed into 'all_cuts_proven_infeasible' below) exists to
                // track, so the master's *bound* is never trusted while
                // that risk is live (see the gate on 'all_cuts_proven_infeasible'
                // above, and 'check_cuts_against_best_solution').
                all_bins_feasible = false;
                std::vector<MinimalInfeasibleSubset> minimal_selections
                    = enumerate_minimal_infeasible_subsets(
                            instance,
                            master_bin.bin_type_id,
                            selected_items,
                            sub_output.is_proven_infeasible,
                            parameters,
                            parameters.maximum_number_of_no_good_cuts_per_bin);
                for (const MinimalInfeasibleSubset& minimal_selection: minimal_selections) {
                    ResourceCut resource_cut;
                    ItemPos cut_size = 0;
                    for (const std::pair<ItemTypeId, ItemPos>& p: minimal_selection.items) {
                        cut_size += p.second;
                        resource_cut.consumption.push_back({p.first, threshold_schedule(p.second)});
                    }
                    resource_cut.capacity = (double)cut_size - 1;

                    // Sequentially lifted into a single, stronger cut (see
                    // 'lift_no_good_cut').
                    ResourceCut lifted_cut = lift_no_good_cut(
                            resource_cut, instance, master_bin.bin_type_id, parameters);
                    no_good_cuts_by_bin_type[master_bin.bin_type_id].push_back(std::move(lifted_cut));
                    all_cuts_proven_infeasible
                        = all_cuts_proven_infeasible && minimal_selection.proven;
                }
            }
        }

        // Update solution.
        if (solution.number_of_bins() > 0) {
            std::stringstream ss;
            ss << "BD it " << output.number_of_iterations;
            algorithm_formatter.update_solution(solution, ss.str());
        }

        // Every feasibility subproblem encountered so far (in this
        // iteration and every previous one) has been solved to proven
        // optimality: every cut accumulated so far is therefore sound (see
        // 'check_cuts_against_best_solution'), so none of them should ever
        // exclude the best solution found so far.
        if (all_cuts_proven_infeasible) {
            check_cuts_against_best_solution(
                    instance,
                    output.solution_pool.best(),
                    dff_cuts_by_bin_type,
                    no_good_cuts_by_bin_type);
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
