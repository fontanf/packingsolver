#include "irregular/large_item_first.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

#include "shape/convex_hull.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

LargeItemFirstOutput packingsolver::irregular::large_item_first(
        const Instance& instance,
        const LargeItemFirstParameters& parameters)
{
    LargeItemFirstOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Compute the convex hull area of each non-fixed item type and find the
    // maximum.
    AreaDbl max_non_fixed_convex_hull_area = 0.0;
    std::vector<AreaDbl> item_type_convex_hull_areas(instance.number_of_item_types(), 0.0);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies <= item_type.copies_fixed)
            continue;
        for (const ItemShape& item_shape: item_type.shapes) {
            Shape convex_hull = shape::convex_hull(item_shape.shape_scaled.shape);
            item_type_convex_hull_areas[item_type_id] += convex_hull.compute_area();
        }
        if (item_type_convex_hull_areas[item_type_id] > max_non_fixed_convex_hull_area)
            max_non_fixed_convex_hull_area = item_type_convex_hull_areas[item_type_id];
    }

    // Classify item types as "large" or "small".
    AreaDbl threshold = max_non_fixed_convex_hull_area / 8.0;
    std::vector<bool> is_large(instance.number_of_item_types(), false);
    bool has_large = false;
    bool has_small = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies <= item_type.copies_fixed)
            continue;
        if (item_type_convex_hull_areas[item_type_id] >= threshold) {
            is_large[item_type_id] = true;
            has_large = true;
        } else {
            has_small = true;
        }
    }

    // If there are no large items or no small items, there is nothing to
    // decompose.
    if (!has_large || !has_small) {
        algorithm_formatter.end();
        return output;
    }

    // Build sub-parameters to pass to each sub-problem.
    auto make_sub_parameters = [&parameters, &algorithm_formatter]() {
        OptimizeParameters sub_parameters;
        sub_parameters.verbosity_level = 0;
        sub_parameters.timer = parameters.timer;
        sub_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sub_parameters.optimization_mode = parameters.optimization_mode;
        sub_parameters.not_anytime_maximum_approximation_ratio
            = parameters.not_anytime_maximum_approximation_ratio;
        sub_parameters.not_anytime_tree_search_queue_size
            = parameters.not_anytime_tree_search_queue_size;
        sub_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
        sub_parameters.use_tree_search = parameters.use_tree_search;
        sub_parameters.use_local_search = parameters.use_local_search;
        sub_parameters.use_milp_raster = parameters.use_milp_raster;
        sub_parameters.use_sequential_single_knapsack = parameters.use_sequential_single_knapsack;
        sub_parameters.use_sequential_value_correction = parameters.use_sequential_value_correction;
        sub_parameters.use_dichotomic_search = parameters.use_dichotomic_search;
        sub_parameters.use_column_generation = parameters.use_column_generation;
        sub_parameters.use_sequential_feasibility = parameters.use_sequential_feasibility;
        return sub_parameters;
    };

    ////////////////////////////////////////////////////////////////////////////
    // Phase 1: solve an instance containing only the large items.
    ////////////////////////////////////////////////////////////////////////////

    InstanceBuilder phase1_instance_builder;
    phase1_instance_builder.set_objective(instance.objective());
    phase1_instance_builder.set_parameters(instance.parameters());

    // Add all bin types (carrying their fixed items).
    std::vector<BinTypeId> phase1_to_orig_bin_type_ids;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        phase1_instance_builder.add_bin_type(instance, bin_type_id, bin_type.copies, bin_type.copies_min);
        phase1_to_orig_bin_type_ids.push_back(bin_type_id);
    }

    // Add large item types to phase 1 and build the inverse mapping
    // phase1_sub_id → original_item_type_id (needed to interpret phase1
    // solution item type IDs when building the phase2 instance).
    std::vector<ItemTypeId> phase1_sub_to_orig_item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (!is_large[item_type_id])
            continue;
        phase1_sub_to_orig_item_type_ids.push_back(item_type_id);
        phase1_instance_builder.add_item_type(instance, item_type_id, item_type.profit, item_type.copies);
    }

    Instance phase1_instance = phase1_instance_builder.build();
    auto phase1_output = optimize(phase1_instance, make_sub_parameters());

    if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end()) {
        algorithm_formatter.end();
        return output;
    }

    const Solution& phase1_solution = phase1_output.solution_pool.best();
    if (phase1_solution.number_of_bins() == 0) {
        algorithm_formatter.end();
        return output;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Phase 2: fix the large items from phase 1 and solve for the small items.
    ////////////////////////////////////////////////////////////////////////////

    // Count how many copies of each large item type were placed in phase 1.
    // solution_item.item_type_id is a phase1 sub-ID; convert to original ID.
    std::vector<ItemPos> large_item_placed_copies(instance.number_of_item_types(), 0);
    for (BinPos bin_pos = 0;
            bin_pos < phase1_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = phase1_solution.bin(bin_pos);
        for (const SolutionItem& solution_item: solution_bin.items) {
            if (!solution_item.is_fixed) {
                ItemTypeId orig_item_type_id
                    = phase1_sub_to_orig_item_type_ids[solution_item.item_type_id];
                large_item_placed_copies[orig_item_type_id] += solution_bin.copies;
            }
        }
    }

    InstanceBuilder phase2_instance_builder;
    phase2_instance_builder.set_objective(instance.objective());
    phase2_instance_builder.set_parameters(instance.parameters());

    // For each bin in the phase 1 solution, add a bin type with the large
    // items fixed at their found positions.
    std::vector<BinTypeId> phase2_to_orig_bin_type_ids;
    for (BinPos bin_pos = 0;
            bin_pos < phase1_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = phase1_solution.bin(bin_pos);
        // Use the original bin type (phase1 bin type IDs match original since
        // all bin types are added in order). This ensures fixed items in the
        // bin type carry original item type IDs, consistent with the item type
        // mapping built below from instance.
        BinTypeId orig_bin_type_id = solution_bin.bin_type_id;
        BinTypeId phase2_bin_type_id = (BinTypeId)phase2_to_orig_bin_type_ids.size();
        phase2_instance_builder.add_bin_type(
                instance,
                orig_bin_type_id,
                solution_bin.copies,
                0);
        phase2_to_orig_bin_type_ids.push_back(orig_bin_type_id);

        // Fix the non-fixed large items at their phase 1 positions.
        // solution_item.item_type_id is a phase1 sub-ID; convert to original.
        for (const SolutionItem& solution_item: solution_bin.items) {
            if (!solution_item.is_fixed) {
                ItemTypeId orig_item_type_id
                    = phase1_sub_to_orig_item_type_ids[solution_item.item_type_id];
                phase2_instance_builder.add_fixed_item(
                        phase2_bin_type_id,
                        orig_item_type_id,
                        solution_item.bl_corner,
                        solution_item.angle,
                        solution_item.mirror);
            }
        }
    }

    // Add item types in the same order. Large items: copies already fixed
    // originally plus copies placed in phase 1. Small items: all copies.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos copies = is_large[item_type_id]
            ? item_type.copies_fixed + large_item_placed_copies[item_type_id]
            : item_type.copies;
        phase2_instance_builder.add_item_type(instance, item_type_id, item_type.profit, copies);
    }

    Instance phase2_instance = phase2_instance_builder.build();
    auto phase2_output = optimize(phase2_instance, make_sub_parameters());

    if (phase2_output.solution_pool.best().number_of_bins() == 0) {
        algorithm_formatter.end();
        return output;
    }

    // Reconstruct and report the solution in the original instance.
    Solution solution(instance);
    solution.append(
            phase2_output.solution_pool.best(),
            phase2_to_orig_bin_type_ids,
            {});
    std::stringstream ss;
    ss << "LIF";
    algorithm_formatter.update_solution(solution, ss.str());

    algorithm_formatter.end();
    return output;
}
