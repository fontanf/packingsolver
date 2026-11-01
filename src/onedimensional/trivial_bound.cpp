#include "onedimensional/trivial_bound.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"
#include "packingsolver/onedimensional/instance_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

void trivial_bound_impl(
        const Instance& instance,
        const TrivialBoundParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    if (instance.objective() == Objective::Knapsack) {
        // 1D continuous relaxation (length-based Dantzig bound): pool the
        // length of all bins together (ignoring the constraint that items
        // must be split across discrete, separate bins), sort items by
        // decreasing profit/length ratio, and greedily fill that pooled
        // capacity, taking the last item fractionally. Always a valid,
        // cheap (O(n log n), no search) upper bound, and much tighter than
        // the trivial "sum of all profits" whenever item profits aren't
        // roughly proportional to their length. Items with a positive
        // nesting length take up less space than their full length once
        // packed next to another copy of the same type, so that reduced
        // length is used instead, matching the bin packing bound below.
        // Items longer than every bin type can never be packed (even a
        // single, un-nested copy needs its full length to fit), so they
        // must be excluded entirely rather than counted as fractionally
        // packable length.
        Length total_capacity = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            total_capacity += bin_type.length * bin_type.copies;
        }
        std::vector<ItemTypeId> sorted_item_types;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (instance.fits_some_bin(item_type_id))
                sorted_item_types.push_back(item_type_id);
        }
        std::sort(
                sorted_item_types.begin(),
                sorted_item_types.end(),
                [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2) -> bool
                {
                    const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                    const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                    Length length_1 = item_type_1.length - std::max(item_type_1.nesting_length, (Length)0);
                    Length length_2 = item_type_2.length - std::max(item_type_2.nesting_length, (Length)0);
                    return item_type_1.profit * length_2
                        > item_type_2.profit * length_1;
                });
        Profit bound = 0.0;
        Length remaining_capacity = total_capacity;
        for (ItemTypeId item_type_id: sorted_item_types) {
            if (remaining_capacity <= 0)
                break;
            const ItemType& item_type = instance.item_type(item_type_id);
            Length length = item_type.length - std::max(item_type.nesting_length, (Length)0);
            if (length <= 0)
                continue;
            Length item_total_length = length * item_type.copies;
            if (item_total_length <= remaining_capacity) {
                bound += item_type.profit * item_type.copies;
                remaining_capacity -= item_total_length;
            } else {
                bound += item_type.profit
                    * ((double)remaining_capacity / length);
                remaining_capacity = 0;
            }
        }
        // This bound ignores resources entirely. A 'penalize' resource with
        // a negative penalty *increases* the reported profit when
        // triggered (see 'Resource'), so add back the worst case - every
        // such resource triggering at once - to keep the bound valid.
        bound += negative_penalty_sum(instance);
        algorithm_formatter.update_knapsack_bound(bound);
        return;
    }

    if (instance.objective() == Objective::BinPacking) {
        // Length-based bound: fill bin types in the order they are
        // provided (as bins are used for this objective) until enough
        // length is available to fit all the items. Cheap (linear in the
        // number of bin/item types). Items with a positive nesting length
        // take up less space than their full length once packed next to
        // another copy of the same type, so that reduced length is used
        // instead.
        Length remaining_item_length = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            remaining_item_length += item_type.copies
                * (item_type.length - std::max(item_type.nesting_length, (Length)0));
        }
        BinPos bound = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            if (remaining_item_length <= 0)
                break;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.length <= 0)
                continue;
            BinPos bins_needed = (BinPos)((remaining_item_length + bin_type.length - 1) / bin_type.length);
            BinPos bins_used = std::min(bins_needed, bin_type.copies);
            bound += bins_used;
            remaining_item_length -= bins_used * bin_type.length;
        }
        algorithm_formatter.update_bin_packing_bound(bound);
        return;
    }

    if (instance.objective() == Objective::VariableSizedBinPacking) {
        // Same bin-selection knapsack as the one solved by the dichotomic
        // search (see 'algorithms/dichotomic_search.hpp'), but solved once
        // at zero waste instead of iteratively: a real solution can never
        // have negative waste, so the minimum cost of a bin selection whose
        // total length covers the (nesting-reduced) item lengths exactly is
        // a valid lower bound on the actual solution's cost.
        //
        // Bin copies beyond 'copies_min' are knapsack items (weight =
        // length, profit = cost); leaving a bin out of the knapsack means
        // using it, so maximizing the profit (cost) of the bins put in the
        // knapsack (i.e. left unused) is equivalent to minimizing the cost
        // of the bins actually used.
        Length bin_length = 0;
        Length bin_min_length = 0;
        Profit total_cost = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            bin_length += bin_type.length * bin_type.copies;
            bin_min_length += bin_type.length * bin_type.copies_min;
            total_cost += bin_type.cost * bin_type.copies;
        }
        Length item_length = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            item_length += item_type.copies
                * (item_type.length - std::max(item_type.nesting_length, (Length)0));
        }

        // The bins used must cover the item lengths, and the mandatory ones
        // (copies_min) hold items too: the optional bins left unused can
        // total up to 'bin_length - bin_min_length' minus what the optional
        // bins used must still cover, 'item_length - bin_min_length' (if
        // positive), i.e. 'bin_length - max(bin_min_length, item_length)'.
        // (Requiring the optional bins alone to cover the item lengths
        // overestimated the bound.)
        Length kp_capacity = bin_length - (std::max)(bin_min_length, item_length);
        if (kp_capacity <= 0) {
            // No bin can be left unused: they are all needed just to cover
            // the item lengths.
            algorithm_formatter.update_variable_sized_bin_packing_bound(total_cost);
            return;
        }

        InstanceBuilder kp_instance_builder;
        kp_instance_builder.set_objective(Objective::Knapsack);
        kp_instance_builder.add_bin_type(kp_capacity);
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            BinPos optional_copies = bin_type.copies - bin_type.copies_min;
            if (optional_copies <= 0
                    || bin_type.length <= 0
                    || bin_type.length > kp_capacity) {
                continue;
            }
            ItemTypeId kp_item_type_id = kp_instance_builder.add_item_type(bin_type.length);
            kp_instance_builder.set_item_type_profit(kp_item_type_id, bin_type.cost);
            kp_instance_builder.set_item_type_copies(kp_item_type_id, optional_copies);
        }
        Instance kp_instance = kp_instance_builder.build();

        OptimizeParameters kp_parameters;
        kp_parameters.verbosity_level = 0;
        kp_parameters.timer = parameters.timer;
        kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        kp_parameters.optimization_mode = OptimizationMode::NotAnytime;
        kp_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        kp_parameters.dynamic_programming_subset_sum_maximum_size
            = parameters.dynamic_programming_subset_sum_maximum_size;
        kp_parameters.dynamic_programming_knapsack_maximum_number_of_states
            = parameters.dynamic_programming_knapsack_maximum_number_of_states;
        auto kp_output = optimize(kp_instance, kp_parameters);

        // The knapsack bound rather than the best solution, since the
        // knapsack may not have been solved to optimality.
        algorithm_formatter.update_variable_sized_bin_packing_bound(
                total_cost - kp_output.knapsack_bound);
        return;
    }
}

}

onedimensional::Output packingsolver::onedimensional::trivial_bound(
        const Instance& instance,
        const TrivialBoundParameters& parameters)
{
    onedimensional::Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    trivial_bound_impl(instance, parameters, algorithm_formatter);

    algorithm_formatter.end();
    return output;
}
