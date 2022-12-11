#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{

template <typename Instance, typename Solution>
using Vbpp2BppFunction = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

template <typename Instance, typename Solution>
struct Vbpp2BppOutput
{
    /** Constructor. */
    Vbpp2BppOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;
};

template <typename Instance, typename Solution>
struct Vbpp2BppOptionalParameters
{
    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();
};

template <typename Instance, typename Solution>
Vbpp2BppOutput<Instance, Solution> vbpp2bpp(
        const Instance& instance,
        const Vbpp2BppFunction<Instance, Solution>& function,
        Vbpp2BppOptionalParameters<Instance, Solution> parameters = {});

template <typename Instance, typename Solution>
Vbpp2BppOutput<Instance, Solution> vbpp2bpp(
        const Instance& instance,
        const Vbpp2BppFunction<Instance, Solution>& function,
        Vbpp2BppOptionalParameters<Instance, Solution> parameters)
{
    Vbpp2BppOutput<Instance, Solution> output(instance);

    // Build PackingSolver Bin Packing instance.
    //std::cout << "Build Bin Packing instance..." << std::endl;
    Instance bpp_instance;
    bpp_instance.set_objective(Objective::BinPacking);
    bpp_instance.set_parameters(instance.parameters());
    // Add all items.
    for (ItemTypeId j = 0; j < instance.number_of_item_types(); ++j) {
        const auto& item_type = instance.item_type(j);
        bpp_instance.add_item_type(item_type, item_type.profit, item_type.copies);
    }
    std::vector<ItemTypeId> item_types_bpp2ps(instance.number_of_item_types());
    std::iota(item_types_bpp2ps.begin(), item_types_bpp2ps.end(), 0);
    // Add all bins.
    for (BinTypeId i = 0; i < instance.number_of_bin_types(); ++i) {
        const auto& bin_type = instance.bin_type(i);
        bpp_instance.add_bin_type(instance.bin_type(i), bin_type.copies);
    }
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
