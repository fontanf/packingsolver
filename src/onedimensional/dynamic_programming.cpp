#include "onedimensional/dynamic_programming.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"
#include "onedimensional/solution_builder.hpp"

#include "knapsacksolver/instance_builder.hpp"
#include "knapsacksolver/algorithms/dynamic_programming_primal_dual.hpp"
#include "subsetsumsolver/instance_builder.hpp"
#include "subsetsumsolver/algorithms/dynamic_programming_bellman.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

namespace
{

void dynamic_programming_impl(
        const Instance& instance,
        const DynamicProgrammingParameters& parameters,
        AlgorithmFormatter& algorithm_formatter)
{
    const BinType& bin_type = instance.bin_type(0);
    knapsacksolver::Weight kp_capacity = bin_type.length;

    // True iff the profit of each item is equal to its weight: then, the
    // knapsack problem is a subset sum problem.
    bool subset_sum = true;
    for (ItemTypeId item_type_id: bin_type.item_type_ids) {
        const ItemType& item_type = instance.item_type(item_type_id);
        Length weight = item_type.length
            - (std::max)(item_type.nesting_length, (Length)0);
        if (item_type.profit != (Profit)weight) {
            subset_sum = false;
            break;
        }
    }

    // Items of the knapsack problem, and knapsack instance.
    knapsacksolver::InstanceFromFloatProfitsBuilder kp_instance_builder;
    kp_instance_builder.set_capacity(kp_capacity);
    std::vector<std::pair<ItemTypeId, ItemPos>> kp2ps;
    for (ItemTypeId item_type_id: bin_type.item_type_ids) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos total_copies = 0;
        ItemPos copies = 1;
        while (total_copies < item_type.copies) {
            if (total_copies + copies > item_type.copies)
                copies = item_type.copies - total_copies;
            knapsacksolver::Weight kp_weight = copies * (
                    item_type.length
                    - (std::max)(item_type.nesting_length, (Length)0));
            if (kp_weight > kp_capacity)
                break;
            kp2ps.push_back({item_type_id, copies});
            kp_instance_builder.add_item(
                    copies * item_type.profit,
                    kp_weight);
            total_copies += copies;
            copies *= 2;
        }
    }

    // Build the solution made of the selected items of the knapsack problem.
    auto build_solution = [&instance, &kp2ps](
            const std::vector<bool>& kp_selected)
    {
        SolutionBuilder solution_builder(instance);
        solution_builder.add_bin(0, 1);
        for (knapsacksolver::ItemId kp_item_id = 0;
                kp_item_id < (knapsacksolver::ItemId)kp2ps.size();
                ++kp_item_id) {
            if (kp_selected[kp_item_id]) {
                ItemTypeId item_type_id = kp2ps[kp_item_id].first;
                ItemPos copies = kp2ps[kp_item_id].second;
                for (ItemPos copy = 0; copy < copies; ++copy)
                    solution_builder.add_item(0, item_type_id);
            }
        }
        return solution_builder.build();
    };

    if (subset_sum) {
        // On subset sum instances, the bounds of the primal-dual dynamic
        // programming don't prune any state, so that its number of states may
        // grow with the capacity. The word RAM dynamic programming uses
        // 'capacity / 64' words, the weights and the capacity being divided
        // by the greatest common divisor of the weights. It is used if its
        // size, the number of items times the number of words of the
        // capacity, which estimates its running time, is small enough.
        auto kp_weight = [&instance, &kp2ps](knapsacksolver::ItemId kp_item_id)
        {
            const ItemType& item_type = instance.item_type(kp2ps[kp_item_id].first);
            return kp2ps[kp_item_id].second * (
                    item_type.length
                    - (std::max)(item_type.nesting_length, (Length)0));
        };
        Counter number_of_items = kp2ps.size();
        Length gcd = 0;
        for (knapsacksolver::ItemId kp_item_id = 0;
                kp_item_id < number_of_items;
                ++kp_item_id) {
            Length weight = kp_weight(kp_item_id);
            while (weight != 0) {
                Length remainder = gcd % weight;
                gcd = weight;
                weight = remainder;
            }
        }
        if (gcd == 0)
            gcd = 1;

        if (number_of_items * ((kp_capacity / gcd) / 64 + 1)
                <= parameters.subset_sum_maximum_size) {
            subsetsumsolver::InstanceBuilder sss_instance_builder;
            sss_instance_builder.set_capacity(kp_capacity);
            for (knapsacksolver::ItemId kp_item_id = 0;
                    kp_item_id < number_of_items;
                    ++kp_item_id) {
                sss_instance_builder.add_item(kp_weight(kp_item_id));
            }
            subsetsumsolver::Instance sss_instance = sss_instance_builder.build();
            subsetsumsolver::Parameters sss_parameters;
            sss_parameters.verbosity_level = 0;
            sss_parameters.timer = parameters.timer;
            sss_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            auto sss_output = subsetsumsolver::dynamic_programming_bellman_word_ram_rec(
                    sss_instance,
                    sss_parameters);
            // When interrupted, the dynamic programming returns without
            // updating its bound to the value of its solution.
            if (sss_output.solution.weight() != sss_output.bound)
                return;
            std::vector<bool> kp_selected(number_of_items, false);
            for (subsetsumsolver::ItemId sss_item_id = 0;
                    sss_item_id < sss_instance.number_of_items();
                    ++sss_item_id) {
                kp_selected[sss_item_id] = sss_output.solution.contains(sss_item_id);
            }
            Solution solution = build_solution(kp_selected);
            if (solution.feasible())
                algorithm_formatter.update_solution(solution, "subset sum");
            algorithm_formatter.update_knapsack_bound(sss_output.solution.weight());
            return;
        }
    }

    knapsacksolver::Instance kp_instance = kp_instance_builder.build();
    knapsacksolver::DynamicProgrammingPrimalDualParameters kp_parameters;
    kp_parameters.verbosity_level = 0;
    kp_parameters.timer = parameters.timer;
    kp_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    // The items here always come from a bounded-multiplicity item type
    // encoded as 0/1 items via binary/geometric decomposition (see the
    // 'copies *= 2' loop above): every chunk of one original item type
    // shares the exact same profit/weight ratio, so none of their achievable
    // partial sums dominate one another and the algorithm's live-state list
    // can blow up to encompass nearly every combination instead of staying
    // small. 'pairing' is an existing (off-by-default, no-op below its size
    // thresholds) mechanism in knapsacksolver's own algorithm for pulling in
    // a promising item from outside the current core to close the value/bound
    // gap once the state list grows large - exactly this situation - so it's
    // enabled unconditionally here rather than only for this instance shape,
    // since it costs nothing when the list stays small.
    kp_parameters.pairing = true;
    kp_parameters.maximum_number_of_states = parameters.knapsack_maximum_number_of_states;
    auto kp_output = knapsacksolver::dynamic_programming_primal_dual(
            kp_instance,
            kp_parameters);
    std::vector<bool> kp_selected(kp2ps.size(), false);
    for (knapsacksolver::ItemId kp_item_id = 0;
            kp_item_id < kp_instance.number_of_items();
            ++kp_item_id) {
        kp_selected[kp_item_id] = kp_output.solution.contains(kp_item_id);
    }
    Solution solution = build_solution(kp_selected);
    if (solution.feasible())
        algorithm_formatter.update_solution(solution, "primal-dual");
    // When interrupted, the dynamic programming returns its current best
    // solution, which is not a bound.
    if (kp_output.solution.profit() == kp_output.bound)
        algorithm_formatter.update_knapsack_bound(solution.profit());
}

}

DynamicProgrammingOutput packingsolver::onedimensional::dynamic_programming(
        const Instance& instance,
        const DynamicProgrammingParameters& parameters)
{
    if (instance.number_of_bins() != 1
            || instance.objective() != Objective::Knapsack) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + "; "
                "the instance must have a single bin and the Knapsack objective.");
    }

    DynamicProgrammingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    dynamic_programming_impl(instance, parameters, algorithm_formatter);

    algorithm_formatter.end();
    return output;
}
