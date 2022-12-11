#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "knapsacksolver/algorithms/dynamic_programming_primal_dual.hpp"

namespace packingsolver
{

template <typename Instance>
double largest_bin_space(const Instance& instance)
{
    double space_max = 0;
    for (BinTypeId i = 0; i < instance.number_of_bin_types(); ++i)
        if (space_max < instance.bin_type(i).space())
            space_max = instance.bin_type(i).space();
    return space_max;
}

template <typename Instance>
double mean_item_space(const Instance& instance)
{
    double space = 0;
    for (ItemTypeId j = 0; j < instance.number_of_item_types(); ++j)
        space += instance.item_type(j).copies * instance.item_type(j).space();
    space /= instance.number_of_items();
    return space;
}

template <typename Instance, typename Solution>
using DichotomicSearchFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename Solution>
struct DichotomicSearchOutput
{
    /** Constructor. */
    DichotomicSearchOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;

    /** Lower bound on the waste percentage. */
    double waste_percentage_lower_bound = 0;

    /** Final waste percentage. */
    double waste_percentage;

    /** Upper bound on the waste percentage. */
    double waste_percentage_upper_bound = std::numeric_limits<double>::infinity();
};

template <typename Instance, typename Solution>
using DichotomicSearchNewSolutionCallback = std::function<void(const DichotomicSearchOutput<Instance, Solution>&)>;

template <typename Instance, typename Solution>
struct DichotomicSearchOptionalParameters
{
    /** Initial waste percentage. */
    double initial_waste_percentage = 0.1;

    /** New solution callback. */
    DichotomicSearchNewSolutionCallback<Instance, Solution> new_solution_callback
        = [](const DichotomicSearchOutput<Instance, Solution>&) { };

    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();
};

template <typename Instance, typename Solution>
DichotomicSearchOutput<Instance, Solution> dichotomic_search(
        const Instance& instance,
        const DichotomicSearchFunction<Instance, Solution>& function,
        DichotomicSearchOptionalParameters<Instance, Solution> parameters = {});

template <typename Instance, typename Solution>
DichotomicSearchOutput<Instance, Solution> dichotomic_search(
        const Instance& instance,
        const DichotomicSearchFunction<Instance, Solution>& function,
        DichotomicSearchOptionalParameters<Instance, Solution> parameters)
{
    DichotomicSearchOutput<Instance, Solution> output(instance);

    // Compute item_space.
    auto item_space = instance.item_type(0).space();
    item_space = 0;
    for (ItemTypeId j = 0; j < instance.number_of_item_types(); ++j)
        item_space += instance.item_type(j).copies * instance.item_type(j).space();
    // Compute bin_min_space.
    auto bin_min_space = instance.bin_type(0).space();
    bin_min_space = 0;
    for (BinTypeId i = 0; i < instance.number_of_bin_types(); ++i)
        bin_min_space += instance.bin_type(i).copies_min * instance.bin_type(i).space();
    //std::cout << bin_min_space << std::endl;
    // Compute bin_space.
    auto bin_space = instance.bin_type(0).space();
    bin_space = 0;
    for (BinTypeId i = 0; i < instance.number_of_bin_types(); ++i)
        bin_space += instance.bin_type(i).copies * instance.bin_type(i).space();
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

    output.waste_percentage = parameters.initial_waste_percentage;
    std::map<std::vector<BinTypeId>, bool> memory;
    while (output.waste_percentage_upper_bound - output.waste_percentage_lower_bound > 0.00001) {
        //std::cout << output.waste_percentage << std::endl;

        if (parameters.info.needs_to_end())
            break;

        // Build knapsack instance.
        //std::cout << "Build Knapsack instance..." << std::endl;
        knapsacksolver::Instance kp_instance;
        // Set knapsack capacity.
        //std::cout << "bin_space " << bin_space << std::endl;
        //std::cout << "item_space " << item_space << std::endl;
        kp_instance.set_capacity(
                bin_space - bin_min_space - (item_space * (1 + output.waste_percentage)));
        // Add knapsack items which are PackingSolver bins.
        std::vector<BinTypeId> kp2ps;
        for (BinTypeId i = 0; i < instance.number_of_bin_types(); ++i) {
            const auto& bin_type = instance.bin_type(i);
            for (BinPos pos = 0; pos < bin_type.copies; ++pos) {
                kp_instance.add_item(bin_type.space(), bin_type.cost);
                kp2ps.push_back(i);
            }
        }
        //std::cout << "kp2ps";
        //for (BinTypeId i: kp2ps)
        //    std::cout << " " << i;
        //std::cout << std::endl;
        // Solve knapsack instance.
        knapsacksolver::DynamicProgrammingPrimalDualOptionalParameters kp_parameters;
        kp_parameters.info = Info(parameters.info, false, "knapsack");
        //kp_parameters.info.set_verbosity_level(2);
        auto kp_output = knapsacksolver::dynamic_programming_primal_dual(kp_instance, kp_parameters);

        // Build PackingSolver Bin Packing instance.
        //std::cout << "Build Bin Packing instance..." << std::endl;
        Instance bpp_instance;
        bpp_instance.set_objective(Objective::BinPacking);
        bpp_instance.set_parameters(instance.parameters());
        // Add all items.
        for (ItemTypeId j = 0; j < instance.number_of_item_types(); ++j) {
            const auto& item = instance.item_type(j);
            bpp_instance.add_item_type(item, item.profit, item.copies);
        }
        std::vector<ItemTypeId> item_types_bpp2ps(instance.number_of_item_types());
        std::iota(item_types_bpp2ps.begin(), item_types_bpp2ps.end(), 0);
        // Retrieve bins based on knapsack solution.
        std::vector<BinTypeId> bin_copies(instance.number_of_bin_types(), 0);
        for (BinTypeId i = 0; i < instance.number_of_bin_types(); ++i)
            bin_copies[i] += instance.bin_type(i).copies_min;
        for (knapsacksolver::ItemIdx j_kp = 0; j_kp < kp_instance.number_of_items(); ++j_kp) {
            //std::cout << "j_kp " << j_kp << " sol " << kp_output.solution.contains(j_kp) << std::endl;
            if (kp_output.solution.contains_idx(j_kp) == 0)
                bin_copies[kp2ps[j_kp]]++;
        }
        if (memory.find(bin_copies) == memory.end()) {
            // Add bins.
            std::vector<BinTypeId> bin_types_bpp2ps;
            for (BinTypeId i: sorted_bin_types) {
                if (bin_copies[i] > 0) {
                    //std::cout << "i " << i << " " << bin_copies[i] << " " << instance.bin_type(i).space() << std::endl;
                    bpp_instance.add_bin_type(instance.bin_type(i), bin_copies[i]);
                    bin_types_bpp2ps.push_back(i);
                }
            }
            // Solve PackingSolver Bin Packing instance.
            auto bpp_solution = function(bpp_instance).best();

            if (bpp_solution.number_of_items() == instance.number_of_items()) {
                //std::cout << "Solution found" << std::endl;
                // Save solution.
                Solution solution(instance);
                //std::cout << bpp_solution.number_of_items() << " " << bpp_solution.number_of_bins() << std::endl;
                solution.append(bpp_solution, bin_types_bpp2ps, item_types_bpp2ps);
                //std::cout << solution.number_of_items() << " " << solution.number_of_bins() << std::endl;
                //std::cout << &instance << std::endl;
                std::stringstream ss;
                ss << "waste percentage " << output.waste_percentage;
                output.solution_pool.add(solution, ss, parameters.info);
                parameters.new_solution_callback(output);
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
        }
        //std::cout << "Update waste percentage..." << std::endl;
        if (output.waste_percentage_upper_bound == std::numeric_limits<double>::infinity()) {
            output.waste_percentage = output.waste_percentage_lower_bound * 2;
        } else {
            output.waste_percentage = (output.waste_percentage_lower_bound + output.waste_percentage_upper_bound) / 2;
        }
    }

    return output;
}

}
