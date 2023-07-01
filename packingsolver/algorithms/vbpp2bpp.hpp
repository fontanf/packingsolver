/**
 * VBPP2BPP algorithm
 *
 * Algorithm for Variable-sized Bin Packing problems.
 *
 * The algorithm just fixes an order for the bins and solves a Bin Packing
 * subproblem.
 *
 * This algorithm is used to quickly get an initial solution before running the
 * column generation algorithm.
 */

#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{

template <typename Instance, typename InstanceBuilder, typename Solution>
using Vbpp2BppFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename InstanceBuilder, typename Solution>
struct Vbpp2BppOutput
{
    /** Constructor. */
    Vbpp2BppOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;
};

template <typename Instance, typename InstanceBuilder, typename Solution>
struct Vbpp2BppOptionalParameters
{
    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();
};

template <typename Instance, typename InstanceBuilder, typename Solution>
Vbpp2BppOutput<Instance, InstanceBuilder, Solution> vbpp2bpp(
        const Instance& instance,
        const Vbpp2BppFunction<Instance, InstanceBuilder, Solution>& function,
        Vbpp2BppOptionalParameters<Instance, InstanceBuilder, Solution> parameters = {});

template <typename Instance, typename InstanceBuilder, typename Solution>
Vbpp2BppOutput<Instance, InstanceBuilder, Solution> vbpp2bpp(
        const Instance& instance,
        const Vbpp2BppFunction<Instance, InstanceBuilder, Solution>& function,
        Vbpp2BppOptionalParameters<Instance, InstanceBuilder, Solution> parameters)
{
    Vbpp2BppOutput<Instance, InstanceBuilder, Solution> output(instance);

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
    // Add all bins.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance.bin_type(bin_type_id);
        bpp_instance_builder.add_bin_type(
                instance.bin_type(bin_type_id),
                bin_type.copies);
    }
    Instance bpp_instance = bpp_instance_builder.build();
    std::vector<ItemTypeId> bin_types_bpp2ps(instance.number_of_bin_types());
    std::iota(bin_types_bpp2ps.begin(), bin_types_bpp2ps.end(), 0);
    // Solve PackingSolver Bin Packing instance.
    //std::cout << "Solve BPP instance..." << std::endl;
    auto bpp_solution = function(bpp_instance).best();

    // Update waste_percentage.
    if (bpp_solution.number_of_items() == instance.number_of_items()) {
        //std::cout << "Solution found" << std::endl;
        // Save solution.
        Solution solution(instance);
        //std::cout << bpp_solution.number_of_items() << " " << bpp_solution.number_of_bins() << std::endl;
        solution.append(bpp_solution, bin_types_bpp2ps, item_types_bpp2ps);
        //std::cout << solution.number_of_items() << " " << solution.number_of_bins() << std::endl;
        std::stringstream ss;
        output.solution_pool.add(solution, ss, parameters.info);
    }
    return output;
}

}
