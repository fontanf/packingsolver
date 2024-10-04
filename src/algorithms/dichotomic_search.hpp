/**
 * Dichotomic search algorithm
 *
 * Algorithm for variable-sized bin packing problems.
 *
 * The algorithm estimates the waste of the solution and deduces the bins to
 * use to reach this quantity of waste. Then it solves a Bin Packing subproblem
 * with all the selected bins. If it manages to pack all items in the selected
 * bins, then it decreases the waste estimate; otherwise, it increases it.
 *
 * This algorithm works well for problems with many types of bins, or with bins
 * in which many items can fit. In particular, it works much better than the
 * Column Generation algorithm that struggles in these cases.
 */

#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "knapsacksolver/knapsack/instance_builder.hpp"
#include "knapsacksolver/knapsack/algorithms/dynamic_programming_primal_dual.hpp"

namespace packingsolver
{

template <typename Instance>
double mean_item_space(const Instance& instance)
{
    double space = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        space += instance.item_type(item_type_id).copies * instance.item_type(item_type_id).space();
    }
    space /= instance.number_of_items();
    return space;
}

template <typename Instance>
double mean_item_type_copies(const Instance& instance)
{
    ItemPos number_of_copies = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        number_of_copies += instance.item_type(item_type_id).copies;
    }
    return (double)number_of_copies / instance.number_of_item_types();
}

template <typename Instance, typename Solution>
using DichotomicSearchFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename Solution>
struct DichotomicSearchOutput: Output<Instance, Solution>
{
    /** Constructor. */
    DichotomicSearchOutput(const Instance& instance):
        Output<Instance, Solution>(instance) { }


    /** Lower bound on the waste percentage. */
    double waste_percentage_lower_bound = 0;

    /** Final waste percentage. */
    double waste_percentage;

    /** Upper bound on the waste percentage. */
    double waste_percentage_upper_bound = std::numeric_limits<double>::infinity();
};

template <typename Instance, typename Solution>
struct DichotomicSearchParameters: Parameters<Instance, Solution>
{
    /** Initial waste percentage. */
    double initial_waste_percentage = 0.1;

    /** Initial upper bound on the waste percentage. */
    double initial_waste_percentage_upper_bound = std::numeric_limits<double>::infinity();
};

template <typename Instance, typename InstanceBuilder, typename Solution, typename AlgorithmFormatter>
DichotomicSearchOutput<Instance, Solution> dichotomic_search(
        const Instance& instance,
        const DichotomicSearchFunction<Instance, Solution>& function,
        const DichotomicSearchParameters<Instance, Solution>& parameters)
{
    DichotomicSearchOutput<Instance, Solution> output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.number_of_item_types() == 0)
        return output;

    // Compute item_space.
    auto item_space = instance.item_type(0).space();
    item_space = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id)
        item_space += instance.item_type(item_type_id).copies * instance.item_type(item_type_id).space();
    // Compute bin_min_space.
    auto bin_min_space = instance.bin_type(0).space();
    bin_min_space = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id)
        bin_min_space += instance.bin_type(bin_type_id).copies_min * instance.bin_type(bin_type_id).space();
    //std::cout << bin_min_space << std::endl;
    // Compute bin_space.
    auto bin_space = instance.bin_type(0).space();
    bin_space = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id)
        bin_space += instance.bin_type(bin_type_id).copies * instance.bin_type(bin_type_id).space();
    // Compute sorted_bin_types.
    std::vector<BinTypeId> sorted_bin_types(instance.number_of_bin_types());
    std::iota(sorted_bin_types.begin(), sorted_bin_types.end(), 0);
    // Sort bins by increasing size.
    std::sort(
            sorted_bin_types.begin(),
            sorted_bin_types.end(),
            [&instance](BinTypeId i1, BinTypeId i2) -> bool
            {
                return instance.bin_type(i1).space()
                    < instance.bin_type(i2).space();
            });

    output.waste_percentage_upper_bound = parameters.initial_waste_percentage_upper_bound;
    std::map<std::vector<BinTypeId>, bool> memory;
    while (output.waste_percentage_upper_bound - output.waste_percentage_lower_bound > 0.00001) {
        //std::cout << "Update waste percentage..." << std::endl;
        if (output.waste_percentage_upper_bound == std::numeric_limits<double>::infinity()) {
            if (output.waste_percentage_lower_bound == 0) {
                output.waste_percentage = parameters.initial_waste_percentage;
            } else {
                output.waste_percentage = output.waste_percentage_lower_bound * 2;
            }
        } else {
            output.waste_percentage = (output.waste_percentage_lower_bound + output.waste_percentage_upper_bound) / 2;
        }
        //std::cout << output.waste_percentage << std::endl;

        if (parameters.timer.needs_to_end())
            break;

        // Build knapsack instance.
        //std::cout << "Build Knapsack instance..." << std::endl;
        knapsacksolver::knapsack::InstanceBuilder kp_instance_builder;
        // Set knapsack capacity.
        //std::cout << "bin_space " << bin_space << std::endl;
        //std::cout << "bin_min_space " << bin_min_space << std::endl;
        //std::cout << "item_space " << item_space << std::endl;
        knapsacksolver::knapsack::Weight kp_capacity = bin_space
            - bin_min_space
            - (item_space * (1 + output.waste_percentage));
        kp_capacity = std::max(kp_capacity, (knapsacksolver::knapsack::Weight)0);
        kp_instance_builder.set_capacity(kp_capacity);
        // Add knapsack items which are PackingSolver bins.
        std::vector<BinTypeId> kp2ps;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const auto& bin_type = instance.bin_type(bin_type_id);
            if (bin_type.space() > kp_capacity)
                continue;
            for (BinPos pos = 0; pos < bin_type.copies; ++pos) {
                kp_instance_builder.add_item(bin_type.space(), bin_type.cost);
                kp2ps.push_back(bin_type_id);
            }
        }
        const knapsacksolver::knapsack::Instance kp_instance = kp_instance_builder.build();
        //std::cout << "kp2ps";
        //for (BinTypeId i: kp2ps)
        //    std::cout << " " << i;
        //std::cout << std::endl;
        // Solve knapsack instance.
        knapsacksolver::knapsack::DynamicProgrammingPrimalDualParameters kp_parameters;
        kp_parameters.verbosity_level = 0;
        auto kp_output = knapsacksolver::knapsack::dynamic_programming_primal_dual(
                kp_instance,
                kp_parameters);

        // Build PackingSolver Bin Packing instance.
        //std::cout << "Build Bin Packing instance..." << std::endl;
        InstanceBuilder bpp_instance_builder;
        bpp_instance_builder.set_objective(Objective::BinPacking);
        bpp_instance_builder.set_parameters(instance.parameters());
        // Add all items.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const auto& item_type = instance.item_type(item_type_id);
            bpp_instance_builder.add_item_type(
                    item_type,
                    item_type.profit,
                    item_type.copies);
        }
        std::vector<ItemTypeId> item_types_bpp2ps(instance.number_of_item_types());
        std::iota(item_types_bpp2ps.begin(), item_types_bpp2ps.end(), 0);
        // Retrieve bins based on knapsack solution.
        std::vector<BinTypeId> bin_copies(instance.number_of_bin_types(), 0);
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id)
            bin_copies[bin_type_id] += instance.bin_type(bin_type_id).copies_min;
        for (knapsacksolver::knapsack::ItemId kp_item_id = 0;
                kp_item_id < kp_instance.number_of_items();
                ++kp_item_id) {
            //std::cout << "kp_item_id " << kp_item_id << " sol " << kp_output.solution.contains(kp_item_id) << std::endl;
            if (kp_output.solution.contains(kp_item_id) == 0)
                bin_copies[kp2ps[kp_item_id]]++;
        }
        if (memory.find(bin_copies) == memory.end()) {
            // Add bins.
            std::vector<BinTypeId> bin_types_bpp2ps;
            BinPos number_of_bins = 0;
            for (BinTypeId bin_type_id: sorted_bin_types) {
                if (bin_copies[bin_type_id] > 0) {
                    //std::cout << "bin_type_id " << i << " " << bin_copies[i] << " " << instance.bin_type(i).space() << std::endl;
                    bpp_instance_builder.add_bin_type(
                            instance.bin_type(bin_type_id),
                            bin_copies[bin_type_id]);
                    bin_types_bpp2ps.push_back(bin_type_id);
                    number_of_bins += bin_copies[bin_type_id];
                }
            }
            for (BinTypeId bin_type_id: sorted_bin_types) {
                const auto& bin_type = instance.bin_type(bin_type_id);
                auto copies = bin_type.copies - bin_copies[bin_type_id];
                if (copies > 0) {
                    //std::cout << "bin_type_id " << i << " " << bin_copies[i] << " " << instance.bin_type(i).space() << std::endl;
                    bpp_instance_builder.add_bin_type(
                            instance.bin_type(bin_type_id),
                            copies);
                    bin_types_bpp2ps.push_back(bin_type_id);
                }
            }
            Instance bpp_instance = bpp_instance_builder.build();
            // Solve PackingSolver Bin Packing instance.
            auto bpp_solution = function(bpp_instance).best();

            // Save solution.
            Solution solution(instance);
            //std::cout << bpp_solution.number_of_items() << " " << bpp_solution.number_of_bins() << std::endl;
            solution.append(bpp_solution, bin_types_bpp2ps, item_types_bpp2ps);
            //std::cout << solution.number_of_items() << " " << solution.number_of_bins() << std::endl;
            //std::cout << &instance << std::endl;
            std::stringstream ss;
            ss << "waste percentage " << output.waste_percentage;
            algorithm_formatter.update_solution(solution, ss.str());

            if (bpp_solution.number_of_items() == instance.number_of_items()
                    && bpp_solution.number_of_bins() <= number_of_bins) {
                //std::cout << "Solution found" << std::endl;
                memory[bin_copies] = true;
            } else {
                //std::cout << "No solution found" << std::endl;
                memory[bin_copies] = false;
            }
        }

        // Update waste_percentage.
        if (memory.at(bin_copies)) {
            output.waste_percentage_upper_bound = output.waste_percentage;
        } else {
            output.waste_percentage_lower_bound = output.waste_percentage;
            if (kp_output.solution.number_of_items() == instance.number_of_bins())
                break;
        }
    }

    algorithm_formatter.end();
    return output;
}

}
