#include "irregular/benders_decomposition.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/milp_nfp.hpp"

#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <set>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * A no-good cut, turned into a onedimensional resource of a bin type.
 *
 * See 'rectangle::ResourceCut'.
 */
struct ResourceCut
{
    double capacity;
    std::vector<std::pair<ItemTypeId, std::vector<double>>> consumption;
};

/**
 * Per-copy consumption schedule making the contribution of an item type to
 * a resource equal to 'min(count, threshold)'.
 *
 * See 'rectangle::threshold_schedule'.
 */
std::vector<double> threshold_schedule(ItemPos threshold)
{
    std::vector<double> schedule(threshold, 1.0);
    schedule.push_back(0.0);
    return schedule;
}

/**
 * No-good cut forbidding a selection of items, and any selection containing
 * it, in a bin.
 */
ResourceCut no_good_cut(
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items)
{
    ResourceCut cut;
    ItemPos number_of_items = 0;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items) {
        number_of_items += p.second;
        cut.consumption.push_back({p.first, threshold_schedule(p.second)});
    }
    cut.capacity = (double)number_of_items - 1;
    return cut;
}

/** Data of the subproblems kept between iterations. */
struct SubproblemCache
{
    /**
     * Outputs of the MILP NFP algorithm, for each bin type and selection of
     * items, so that the same selection is never checked twice (the minimal
     * infeasible subsets of different bins, and of different iterations,
     * often share subsets).
     */
    std::map<
        std::pair<BinTypeId, std::vector<std::pair<ItemTypeId, ItemPos>>>,
        MilpNfpOutput> outputs;

    /** Data of the MILP NFP algorithm (the pieces of the pairs of items). */
    MilpNfpCache milp_nfp_cache;
};

/** Result of checking whether a selection of items fits in a bin. */
enum class SelectionFeasibility
{
    Feasible,
    ProvenInfeasible,
    Unknown,
};

/**
 * Output of the MILP NFP algorithm for a selection of items, through the
 * cache.
 */
MilpNfpOutput milp_nfp_output(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        const BendersDecompositionParameters& parameters,
        SubproblemCache& subproblem_cache)
{
    auto key = std::make_pair(bin_type_id, selected_items);
    auto it = subproblem_cache.outputs.find(key);
    if (it != subproblem_cache.outputs.end())
        return it->second;
    std::vector<ItemTypeId> item_type_ids;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items)
        for (ItemPos copy = 0; copy < p.second; ++copy)
            item_type_ids.push_back(p.first);
    MilpNfpParameters milp_nfp_parameters;
    milp_nfp_parameters.verbosity_level = 0;
    milp_nfp_parameters.timer = parameters.timer;
    if (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)
        milp_nfp_parameters.number_of_threads = 1;
    milp_nfp_parameters.cache = &subproblem_cache.milp_nfp_cache;
    MilpNfpOutput output = milp_nfp(
            instance,
            bin_type_id,
            item_type_ids,
            milp_nfp_parameters);
    // 'Unknown' may come from the timer, so it isn't stored.
    if (output.status != MilpNfpStatus::Unknown)
        subproblem_cache.outputs[key] = output;
    return output;
}

/**
 * Check whether a selection of items fits in a bin of type 'bin_type_id'.
 *
 * If 'need_packing' is 'true', a selection is only 'Feasible' if a packing
 * is found, and the packing is written in 'packing' (a solution of
 * 'instance' with a single bin). Otherwise, a selection which the MILP NFP
 * algorithm doesn't prove infeasible may also be considered 'Feasible',
 * without a packing.
 */
SelectionFeasibility selection_feasibility(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        bool need_packing,
        const BendersDecompositionParameters& parameters,
        SubproblemCache& subproblem_cache,
        Solution* packing = nullptr)
{
    if (selected_items.empty())
        return SelectionFeasibility::Feasible;

    if (parameters.subproblem_use_milp_nfp) {
        MilpNfpOutput milp_nfp_output_ = milp_nfp_output(
                instance,
                bin_type_id,
                selected_items,
                parameters,
                subproblem_cache);
        if (milp_nfp_output_.status == MilpNfpStatus::Infeasible)
            return SelectionFeasibility::ProvenInfeasible;
        if (milp_nfp_output_.status == MilpNfpStatus::Feasible) {
            if (!need_packing)
                return SelectionFeasibility::Feasible;
            // The solution of the relaxation may be a packing.
            if (packing != nullptr
                    && milp_nfp_solution(
                        instance,
                        bin_type_id,
                        milp_nfp_output_,
                        *packing)) {
                return SelectionFeasibility::Feasible;
            }
        }
    }

    // Build the subproblem instance.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Feasibility);
    sub_instance_builder.set_parameters(instance.parameters());
    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
    sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
    std::vector<ItemTypeId> sub_to_orig;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items) {
        ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(instance, p.first);
        sub_instance_builder.set_item_type_copies(sub_item_type_id, p.second);
        sub_to_orig.push_back(p.first);
    }
    Instance sub_instance = sub_instance_builder.build();

    // Solve it.
    OptimizeParameters sub_parameters;
    sub_parameters.verbosity_level = 0;
    sub_parameters.timer = parameters.timer;
    sub_parameters.optimization_mode
        = (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
        OptimizationMode::NotAnytimeSequential:
        OptimizationMode::NotAnytimeDeterministic;
    sub_parameters.not_anytime_tree_search_queue_size = parameters.subproblem_queue_size;
    sub_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    irregular::Output sub_output = optimize(sub_instance, sub_parameters);
    const Solution& sub_solution = sub_output.solution_pool.best();

    if (sub_solution.feasible()) {
        if (packing != nullptr) {
            *packing = Solution(instance);
            packing->append_bin(sub_solution, 0, 1, {bin_type_id}, sub_to_orig);
        }
        return SelectionFeasibility::Feasible;
    }
    if (sub_output.is_proven_infeasible)
        return SelectionFeasibility::ProvenInfeasible;
    return SelectionFeasibility::Unknown;
}

/**
 * Aggregate a list of items (one entry per copy) into (item type, number of
 * copies) pairs, skipping the removed ones.
 */
std::vector<std::pair<ItemTypeId, ItemPos>> aggregate_units(
        const Instance& instance,
        const std::vector<ItemTypeId>& units,
        const std::vector<bool>& removed)
{
    std::vector<ItemPos> counts(instance.number_of_item_types(), 0);
    for (std::size_t unit_pos = 0; unit_pos < units.size(); ++unit_pos)
        if (!removed[unit_pos])
            counts[units[unit_pos]]++;
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
 * Return 'true' iff 'selected_items' contains at least the copies of
 * 'subset'. Both are sorted by item type.
 */
bool contains(
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& subset)
{
    auto it = selected_items.begin();
    for (const std::pair<ItemTypeId, ItemPos>& p: subset) {
        while (it != selected_items.end() && it->first < p.first)
            ++it;
        if (it == selected_items.end()
                || it->first != p.first
                || it->second < p.second) {
            return false;
        }
    }
    return true;
}

/**
 * Lift a no-good cut forbidding the proven infeasible items 'items' (S) in a
 * bin of type 'bin_type_id': 'sum_{i in S} min(x_i, n_i) <= |S| - 1'.
 *
 * The item types not in S are considered one by one, largest first. An item
 * type j gets the coefficient 1 if every selection made of a copy of j and
 * of |S| - 1 items of S and of the item types already lifted (L) is proven
 * infeasible: then no feasible selection reaches |S| on the lifted cut, so
 * it stays valid. Since the cut is a no-good cut, it means that j can
 * replace any item of S (and of L) in it.
 *
 * A selection containing items forbidden by a proven cut is infeasible
 * without solving it.
 */
ResourceCut lift_no_good_cut(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& items,
        const std::vector<std::vector<std::pair<ItemTypeId, ItemPos>>>& proven_cut_items,
        const BendersDecompositionParameters& parameters,
        SubproblemCache& subproblem_cache,
        BendersDecompositionOutput& output)
{
    ResourceCut cut = no_good_cut(items);
    ItemPos cut_size = 0;
    std::vector<bool> in_cut(instance.number_of_item_types(), false);
    std::vector<ItemTypeId> units;
    for (const std::pair<ItemTypeId, ItemPos>& p: items) {
        cut_size += p.second;
        in_cut[p.first] = true;
        for (ItemPos copy = 0; copy < p.second; ++copy)
            units.push_back(p.first);
    }

    // Return 'true' iff a selection is proven infeasible.
    auto proven_infeasible = [&](const std::vector<std::pair<ItemTypeId, ItemPos>>& selection) -> bool
    {
        for (const auto& cut_items: proven_cut_items)
            if (contains(selection, cut_items))
                return true;
        return milp_nfp_output(
                instance,
                bin_type_id,
                selection,
                parameters,
                subproblem_cache).status == MilpNfpStatus::Infeasible;
    };

    std::vector<ItemTypeId> candidates;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!in_cut[item_type_id])
            candidates.push_back(item_type_id);
    }
    std::stable_sort(
            candidates.begin(),
            candidates.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return instance.item_type(item_type_id_1).area_scaled
                    > instance.item_type(item_type_id_2).area_scaled;
            });

    for (ItemTypeId candidate_id: candidates) {
        if ((Counter)units.size() - cut_size
                >= parameters.maximum_number_of_lifted_item_types) {
            break;
        }
        if (parameters.timer.needs_to_end())
            break;
        // Enumerate the selections of |S| - 1 items of S and L, by the
        // items removed.
        std::vector<bool> removed(units.size(), false);
        std::size_t number_of_removed_units = units.size() - (cut_size - 1);
        std::set<std::vector<std::pair<ItemTypeId, ItemPos>>> checked;
        bool valid = true;
        std::function<void(std::size_t, std::size_t)> enumerate = [&](
                std::size_t unit_pos,
                std::size_t number_of_removed)
        {
            if (!valid)
                return;
            if (number_of_removed == number_of_removed_units) {
                std::vector<ItemTypeId> selection_units = {candidate_id};
                for (std::size_t pos = 0; pos < units.size(); ++pos)
                    if (!removed[pos])
                        selection_units.push_back(units[pos]);
                std::vector<bool> none(selection_units.size(), false);
                std::vector<std::pair<ItemTypeId, ItemPos>> selection
                    = aggregate_units(instance, selection_units, none);
                if (!checked.insert(selection).second)
                    return;
                if (!proven_infeasible(selection))
                    valid = false;
                return;
            }
            if (unit_pos == units.size())
                return;
            removed[unit_pos] = true;
            enumerate(unit_pos + 1, number_of_removed + 1);
            removed[unit_pos] = false;
            enumerate(unit_pos + 1, number_of_removed);
        };
        enumerate(0, 0);
        if (!valid || parameters.timer.needs_to_end())
            continue;
        cut.consumption.push_back({candidate_id, threshold_schedule(1)});
        units.push_back(candidate_id);
        output.number_of_lifted_coefficients++;
    }
    return cut;
}

/**
 * Search for an infeasible subset of a selection of items with the MILP NFP
 * algorithm, by adding its items one by one, largest first. Return the first
 * subset proven infeasible, or an empty one if none is.
 *
 * Proving that a large selection doesn't fit may be much more expensive than
 * proving it for a few of its items, and a small infeasible subset is what
 * the no-good cut needs anyway. The last subset checked is the whole
 * selection, so it is not checked twice (see 'SubproblemCache').
 */
std::vector<std::pair<ItemTypeId, ItemPos>> find_infeasible_prefix(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        const BendersDecompositionParameters& parameters,
        SubproblemCache& subproblem_cache)
{
    std::vector<ItemTypeId> units;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items)
        for (ItemPos copy = 0; copy < p.second; ++copy)
            units.push_back(p.first);
    std::stable_sort(
            units.begin(),
            units.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return instance.item_type(item_type_id_1).area_scaled
                    > instance.item_type(item_type_id_2).area_scaled;
            });

    std::vector<bool> removed(units.size(), true);
    for (std::size_t unit_pos = 0; unit_pos < units.size(); ++unit_pos) {
        removed[unit_pos] = false;
        std::vector<std::pair<ItemTypeId, ItemPos>> prefix
            = aggregate_units(instance, units, removed);
        MilpNfpStatus status = milp_nfp_output(
                instance,
                bin_type_id,
                prefix,
                parameters,
                subproblem_cache).status;
        if (status == MilpNfpStatus::Infeasible)
            return prefix;
        if (status == MilpNfpStatus::Unknown)
            break;
    }
    return {};
}

/**
 * A minimal infeasible subset, and whether every check of its derivation
 * proved infeasibility (see 'rectangle::MinimalInfeasibleSubset').
 */
struct MinimalInfeasibleSubset
{
    std::vector<std::pair<ItemTypeId, ItemPos>> items;
    bool proven;
};

/**
 * DFS step of 'enumerate_minimal_infeasible_subsets': try removing each
 * unit from 'next_unit_pos' onwards, recurse on the selections which are not
 * feasible, and record the current selection if none is.
 */
void enumerate_minimal_infeasible_subsets_dfs(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& units,
        std::vector<bool>& removed,
        std::size_t remaining_count,
        std::size_t next_unit_pos,
        bool path_proven,
        const BendersDecompositionParameters& parameters,
        SubproblemCache& subproblem_cache,
        std::vector<MinimalInfeasibleSubset>& subsets)
{
    bool any_child_infeasible = false;
    for (std::size_t unit_pos = next_unit_pos;
            unit_pos < units.size()
            && (Counter)subsets.size() < parameters.maximum_number_of_no_good_cuts_per_bin
            && remaining_count > 1;
            ++unit_pos) {
        if (parameters.timer.needs_to_end())
            break;
        removed[unit_pos] = true;
        SelectionFeasibility child_feasibility = selection_feasibility(
                instance,
                bin_type_id,
                aggregate_units(instance, units, removed),
                false,
                parameters,
                subproblem_cache);
        if (child_feasibility != SelectionFeasibility::Feasible) {
            any_child_infeasible = true;
            enumerate_minimal_infeasible_subsets_dfs(
                    instance,
                    bin_type_id,
                    units,
                    removed,
                    remaining_count - 1,
                    unit_pos + 1,
                    path_proven && (child_feasibility == SelectionFeasibility::ProvenInfeasible),
                    parameters,
                    subproblem_cache,
                    subsets);
        }
        removed[unit_pos] = false;
    }

    if (!any_child_infeasible
            && (Counter)subsets.size() < parameters.maximum_number_of_no_good_cuts_per_bin) {
        subsets.push_back({aggregate_units(instance, units, removed), path_proven});
    }
}

/**
 * Enumerate minimal infeasible subsets of an infeasible selection of items.
 *
 * The items are sorted by increasing area, so that the small items are
 * removed first.
 */
std::vector<MinimalInfeasibleSubset> enumerate_minimal_infeasible_subsets(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items,
        bool selection_proven_infeasible,
        const BendersDecompositionParameters& parameters,
        SubproblemCache& subproblem_cache)
{
    std::vector<ItemTypeId> units;
    for (const std::pair<ItemTypeId, ItemPos>& p: selected_items)
        for (ItemPos copy = 0; copy < p.second; ++copy)
            units.push_back(p.first);
    std::stable_sort(
            units.begin(),
            units.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return instance.item_type(item_type_id_1).area_scaled
                    < instance.item_type(item_type_id_2).area_scaled;
            });

    std::vector<bool> removed(units.size(), false);
    std::vector<MinimalInfeasibleSubset> subsets;
    enumerate_minimal_infeasible_subsets_dfs(
            instance,
            bin_type_id,
            units,
            removed,
            units.size(),
            0,
            selection_proven_infeasible,
            parameters,
            subproblem_cache,
            subsets);
    return subsets;
}

/** Add a cut as a new resource of a bin type of the master problem. */
void add_cut_as_resource(
        onedimensional::InstanceBuilder& master_instance_builder,
        BinTypeId master_bin_type_id,
        const ResourceCut& cut)
{
    ResourceId resource_id = master_instance_builder.add_bin_type_resource(
            master_bin_type_id, cut.capacity);
    for (const std::pair<ItemTypeId, std::vector<double>>& entry: cut.consumption) {
        master_instance_builder.add_resource_consumption(
                master_bin_type_id,
                resource_id,
                entry.first,
                entry.second);
    }
}

/**
 * Build the master problem: item types and bin types have the same ids as
 * in the original instance.
 *
 * The areas of the item types are rounded down and the areas of the bin
 * types rounded up, so that the master problem remains a relaxation.
 */
onedimensional::Instance build_master_instance(
        const Instance& instance,
        const std::vector<std::vector<ResourceCut>>& cuts_by_bin_type)
{
    onedimensional::InstanceBuilder master_instance_builder;
    master_instance_builder.set_objective(instance.objective());

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeId master_bin_type_id = master_instance_builder.add_bin_type(
                std::ceil(bin_type.area_scaled));
        master_instance_builder.set_bin_type_cost(master_bin_type_id, bin_type.cost);
        master_instance_builder.set_bin_type_copies(master_bin_type_id, bin_type.copies);
        master_instance_builder.set_bin_type_copies_min(master_bin_type_id, bin_type.copies_min);

        for (const ResourceCut& cut: cuts_by_bin_type[bin_type_id])
            add_cut_as_resource(master_instance_builder, master_bin_type_id, cut);

        // Resources of the instance. They are enforced by the master
        // problem, so the subproblems don't need to consider them.
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resources[resource_id];
            ResourceId master_resource_id = master_instance_builder.add_bin_type_resource(
                    master_bin_type_id,
                    resource.capacity,
                    resource.penalize,
                    resource.penalty);
            for (const std::pair<ItemTypeId, std::vector<double>>& entry: resource.item_consumptions) {
                master_instance_builder.add_resource_consumption(
                        master_bin_type_id,
                        master_resource_id,
                        entry.first,
                        entry.second);
            }
        }
    }

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemTypeId master_item_type_id = master_instance_builder.add_item_type(
                std::floor(item_type.area_scaled));
        master_instance_builder.set_item_type_copies(master_item_type_id, item_type.copies);
        master_instance_builder.set_item_type_copies_min(master_item_type_id, item_type.copies_min);
        if (instance.objective() == Objective::Knapsack)
            master_instance_builder.set_item_type_profit(master_item_type_id, item_type.profit);
    }

    return master_instance_builder.build();
}

/**
 * Aggregate the items of a bin of a onedimensional solution into (item type,
 * number of copies) pairs.
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
 * Throw if a cut excludes a bin of 'solution'.
 *
 * Only called while every cut is proven: a feasible solution then satisfies
 * all of them, so a violation means that a cut is wrong.
 */
void check_cuts_against_solution(
        const Instance& instance,
        const Solution& solution,
        const std::vector<std::vector<ResourceCut>>& cuts_by_bin_type)
{
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        std::vector<ItemPos> item_type_counts(instance.number_of_item_types(), 0);
        for (const SolutionItem& item: solution_bin.items)
            item_type_counts[item.item_type_id]++;
        for (const ResourceCut& cut: cuts_by_bin_type[solution_bin.bin_type_id]) {
            double consumption = 0.0;
            for (const std::pair<ItemTypeId, std::vector<double>>& entry: cut.consumption) {
                ItemPos count = item_type_counts[entry.first];
                for (ItemPos copy = 0; copy < count; ++copy) {
                    consumption += (copy < (ItemPos)entry.second.size())?
                        entry.second[copy]:
                        entry.second.back();
                }
            }
            if (packingsolver::strictly_greater(consumption, cut.capacity)) {
                throw std::logic_error(
                        FUNC_SIGNATURE + ": a no-good cut excludes "
                        "the current best solution.");
            }
        }
    }
}

double elapsed_time(std::chrono::steady_clock::time_point begin)
{
    return std::chrono::duration_cast<std::chrono::duration<double>>(
            std::chrono::steady_clock::now() - begin).count();
}

}

BendersDecompositionOutput packingsolver::irregular::benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters)
{
    BendersDecompositionOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() != Objective::Knapsack
            && instance.objective() != Objective::BinPacking
            && instance.objective() != Objective::VariableSizedBinPacking) {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "objective \""
            << instance.objective() << "\" not supported.";
        throw std::invalid_argument(ss.str());
    }

    // No-good cuts, per bin type, and the items they forbid.
    std::vector<std::vector<ResourceCut>> cuts_by_bin_type(instance.number_of_bin_types());
    std::vector<std::vector<std::vector<std::pair<ItemTypeId, ItemPos>>>> cut_items_by_bin_type(
            instance.number_of_bin_types());
    std::vector<std::vector<std::vector<std::pair<ItemTypeId, ItemPos>>>> proven_cut_items_by_bin_type(
            instance.number_of_bin_types());

    SubproblemCache subproblem_cache;

    // 'true' iff every cut added so far is proven. Otherwise, the bound of
    // the master problem is not a bound of the instance.
    bool all_cuts_proven = true;

    // Incompatible pairs and triples of items, proven by the MILP NFP
    // algorithm: they give permanent cuts, instead of being found through
    // the subproblems one by one.
    if (parameters.subproblem_use_milp_nfp
            && parameters.compute_incompatible_pairs) {
        auto incompatible_subsets_begin = std::chrono::steady_clock::now();
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            // Add a proven cut forbidding 'items' in a bin of this type.
            auto add_cut = [&](const std::vector<std::pair<ItemTypeId, ItemPos>>& items)
            {
                cuts_by_bin_type[bin_type_id].push_back(no_good_cut(items));
                cut_items_by_bin_type[bin_type_id].push_back(items);
                proven_cut_items_by_bin_type[bin_type_id].push_back(items);
                output.number_of_no_good_cuts++;
            };
            // Return 'true' iff the items are proven not to fit.
            auto incompatible = [&](const std::vector<std::pair<ItemTypeId, ItemPos>>& items)
            {
                return milp_nfp_output(
                        instance,
                        bin_type_id,
                        items,
                        parameters,
                        subproblem_cache).status == MilpNfpStatus::Infeasible;
            };
            // Aggregate a list of item types (one per copy, sorted).
            auto aggregate = [](const std::vector<ItemTypeId>& item_type_ids)
            {
                std::vector<std::pair<ItemTypeId, ItemPos>> items;
                for (ItemTypeId item_type_id: item_type_ids) {
                    if (!items.empty() && items.back().first == item_type_id) {
                        items.back().second++;
                    } else {
                        items.push_back({item_type_id, 1});
                    }
                }
                return items;
            };

            ItemTypeId n = instance.number_of_item_types();
            // pair_incompatible[i][j], i <= j.
            std::vector<std::vector<bool>> pair_incompatible(n, std::vector<bool>(n, false));
            for (ItemTypeId item_type_id_1 = 0; item_type_id_1 < n; ++item_type_id_1) {
                for (ItemTypeId item_type_id_2 = item_type_id_1; item_type_id_2 < n; ++item_type_id_2) {
                    if (parameters.timer.needs_to_end())
                        break;
                    if (item_type_id_1 == item_type_id_2
                            && instance.item_type(item_type_id_1).copies < 2) {
                        continue;
                    }
                    if (incompatible(aggregate({item_type_id_1, item_type_id_2}))) {
                        pair_incompatible[item_type_id_1][item_type_id_2] = true;
                        add_cut(aggregate({item_type_id_1, item_type_id_2}));
                        output.number_of_incompatible_pairs++;
                    }
                }
            }

            if (!parameters.compute_incompatible_triples)
                continue;
            for (ItemTypeId item_type_id_1 = 0; item_type_id_1 < n; ++item_type_id_1) {
                for (ItemTypeId item_type_id_2 = item_type_id_1; item_type_id_2 < n; ++item_type_id_2) {
                    if (pair_incompatible[item_type_id_1][item_type_id_2])
                        continue;
                    for (ItemTypeId item_type_id_3 = item_type_id_2; item_type_id_3 < n; ++item_type_id_3) {
                        if (parameters.timer.needs_to_end())
                            break;
                        if (pair_incompatible[item_type_id_1][item_type_id_3]
                                || pair_incompatible[item_type_id_2][item_type_id_3]) {
                            continue;
                        }
                        std::vector<std::pair<ItemTypeId, ItemPos>> items
                            = aggregate({item_type_id_1, item_type_id_2, item_type_id_3});
                        bool enough_copies = true;
                        AreaDbl area = 0;
                        for (const std::pair<ItemTypeId, ItemPos>& p: items) {
                            if (instance.item_type(p.first).copies < p.second)
                                enough_copies = false;
                            area += p.second * instance.item_type(p.first).area_scaled;
                        }
                        if (!enough_copies)
                            continue;
                        // Infeasible by area: the master problem already
                        // forbids it.
                        if (area > bin_type.area_scaled)
                            continue;
                        if (incompatible(items)) {
                            add_cut(items);
                            output.number_of_incompatible_triples++;
                        }
                    }
                }
            }
        }
        output.incompatible_subsets_time = elapsed_time(incompatible_subsets_begin);
    }

    for (output.number_of_iterations = 0;
            ;
            ++output.number_of_iterations) {

        // Check the maximum number of iterations.
        if (parameters.optimization_mode != OptimizationMode::Anytime
                && parameters.optimization_mode != OptimizationMode::Exact
                && parameters.not_anytime_maximum_number_of_iterations >= 0
                && output.number_of_iterations >= parameters.not_anytime_maximum_number_of_iterations) {
            break;
        }

        // Build and solve the master problem.
        auto master_begin = std::chrono::steady_clock::now();
        onedimensional::Instance master_instance = build_master_instance(
                instance,
                cuts_by_bin_type);
        onedimensional::OptimizeParameters master_parameters;
        master_parameters.verbosity_level = 0;
        master_parameters.timer = parameters.timer;
        // In 'Exact' mode, the master problem is solved in 'Exact' mode so
        // that its bound is actually proven.
        master_parameters.optimization_mode
            = (parameters.optimization_mode == OptimizationMode::Exact)?
            OptimizationMode::Exact:
            (parameters.optimization_mode == OptimizationMode::NotAnytimeSequential)?
            OptimizationMode::NotAnytimeSequential:
            OptimizationMode::NotAnytimeDeterministic;
        master_parameters.use_milp_assignment = true;
        master_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        onedimensional::Output master_output = onedimensional::optimize(
                master_instance, master_parameters);
        output.master_time += elapsed_time(master_begin);

        // Check end.
        if (parameters.timer.needs_to_end())
            break;

        // Update the bound.
        if (all_cuts_proven) {
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
        if (master_output.is_proven_infeasible)
            break;

        const onedimensional::Solution& master_solution = master_output.solution_pool.best();
        if (master_solution.number_of_bins() == 0
                && instance.number_of_items() > 0
                && instance.objective() != Objective::Knapsack) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "the master problem should not be infeasible.");
        }

        // Stop if the solution of the master problem can't improve the best
        // solution.
        if (output.solution_pool.best().feasible()) {
            if (instance.objective() == Objective::Knapsack) {
                if (master_solution.profit() <= output.solution_pool.best().profit())
                    break;
            } else if (instance.objective() == Objective::BinPacking) {
                if (master_solution.number_of_bins() >= output.solution_pool.best().number_of_bins())
                    break;
            } else if (instance.objective() == Objective::VariableSizedBinPacking) {
                if (master_solution.cost() >= output.solution_pool.best().cost())
                    break;
            }
        }

        // Solve the subproblem of each bin of the solution of the master
        // problem.
        Solution solution(instance);
        bool all_bins_feasible = true;
        bool need_to_end = false;
        for (BinPos master_bin_pos = 0;
                master_bin_pos < master_solution.number_of_different_bins();
                ++master_bin_pos) {
            const onedimensional::SolutionBin& master_bin = master_solution.bin(master_bin_pos);
            std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = aggregate_bin_items(
                    master_bin, instance.number_of_item_types());

            auto subproblem_begin = std::chrono::steady_clock::now();
            Solution packing(instance);
            SelectionFeasibility feasibility = SelectionFeasibility::Unknown;
            // Infeasible subset of the selection, from which the cuts are
            // derived.
            std::vector<std::pair<ItemTypeId, ItemPos>> infeasible_items;
            // The selection may contain items forbidden by a cut added
            // earlier in this iteration, for another bin.
            bool already_cut = false;
            for (std::size_t cut_pos = 0;
                    cut_pos < cut_items_by_bin_type[master_bin.bin_type_id].size();
                    ++cut_pos) {
                if (contains(selected_items, cut_items_by_bin_type[master_bin.bin_type_id][cut_pos])) {
                    already_cut = true;
                    break;
                }
            }
            if (already_cut) {
                all_bins_feasible = false;
                continue;
            }
            if (parameters.subproblem_use_milp_nfp) {
                infeasible_items = find_infeasible_prefix(
                        instance,
                        master_bin.bin_type_id,
                        selected_items,
                        parameters,
                        subproblem_cache);
                if (!infeasible_items.empty())
                    feasibility = SelectionFeasibility::ProvenInfeasible;
            }
            if (infeasible_items.empty()) {
                feasibility = selection_feasibility(
                        instance,
                        master_bin.bin_type_id,
                        selected_items,
                        true,
                        parameters,
                        subproblem_cache,
                        &packing);
                infeasible_items = selected_items;
            }
            output.subproblem_time += elapsed_time(subproblem_begin);

            if (feasibility == SelectionFeasibility::Feasible) {
                // The 'copies' identical bins of the master solution share
                // the same packing.
                solution.append_bin(packing, 0, master_bin.copies);
                continue;
            }
            all_bins_feasible = false;

            // Check end.
            if (parameters.timer.needs_to_end()) {
                need_to_end = true;
                break;
            }

            // Add a no-good cut for each minimal infeasible subset of the
            // infeasible items. A cut which isn't proven may still help the master
            // problem converge, but then its bound isn't a bound anymore.
            auto minimal_infeasible_subsets_begin = std::chrono::steady_clock::now();
            std::vector<MinimalInfeasibleSubset> subsets = enumerate_minimal_infeasible_subsets(
                    instance,
                    master_bin.bin_type_id,
                    infeasible_items,
                    feasibility == SelectionFeasibility::ProvenInfeasible,
                    parameters,
                    subproblem_cache);
            output.minimal_infeasible_subsets_time += elapsed_time(minimal_infeasible_subsets_begin);
            for (const MinimalInfeasibleSubset& subset: subsets) {
                if (parameters.subproblem_use_milp_nfp
                        && subset.proven
                        && parameters.maximum_number_of_lifted_item_types > 0) {
                    auto lifting_begin = std::chrono::steady_clock::now();
                    cuts_by_bin_type[master_bin.bin_type_id].push_back(lift_no_good_cut(
                                instance,
                                master_bin.bin_type_id,
                                subset.items,
                                proven_cut_items_by_bin_type[master_bin.bin_type_id],
                                parameters,
                                subproblem_cache,
                                output));
                    output.lifting_time += elapsed_time(lifting_begin);
                } else {
                    cuts_by_bin_type[master_bin.bin_type_id].push_back(no_good_cut(subset.items));
                }
                cut_items_by_bin_type[master_bin.bin_type_id].push_back(subset.items);
                if (subset.proven)
                    proven_cut_items_by_bin_type[master_bin.bin_type_id].push_back(subset.items);
                output.number_of_no_good_cuts++;
                if (!subset.proven) {
                    output.number_of_unproven_no_good_cuts++;
                    all_cuts_proven = false;
                }
            }
        }

        // Update the solution.
        if (solution.number_of_bins() > 0) {
            std::stringstream ss;
            ss << "BD it " << output.number_of_iterations;
            algorithm_formatter.update_solution(solution, ss.str());
        }

        if (all_cuts_proven) {
            check_cuts_against_solution(
                    instance,
                    output.solution_pool.best(),
                    cuts_by_bin_type);
        }

        if (need_to_end)
            break;

        // Every bin of the solution of the master problem is packed: it is
        // optimal given the cuts.
        if (all_bins_feasible)
            break;
    }

    algorithm_formatter.end();
    return output;
}
