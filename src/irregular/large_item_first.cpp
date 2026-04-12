#include "irregular/large_item_first.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"

#include "shape/convex_hull.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Run both phases of the large-item-first algorithm for a given tree-search
 * queue size.  Returns true if a valid solution was found and reported.
 */
bool large_item_first_for_queue_size(
        const Instance& instance,
        const std::vector<bool>& is_in_phase1,
        const std::vector<ItemTypeId>& phase1_sub_to_orig_item_type_ids,
        const LargeItemFirstParameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        NodeId queue_size,
        double maximum_approximation_ratio)
{
    std::cout << "queue_size " << queue_size << std::endl;

    ////////////////////////////////////////////////////////////////////////////
    // Phase 1: solve an instance containing only the large items.
    ////////////////////////////////////////////////////////////////////////////

    InstanceBuilder phase1_instance_builder;
    phase1_instance_builder.set_objective(instance.objective());
    phase1_instance_builder.set_parameters(instance.parameters());

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        phase1_instance_builder.add_bin_type(instance, bin_type_id, bin_type.copies, bin_type.copies_min);
    }

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!is_in_phase1[item_type_id])
            continue;
        const ItemType& item_type = instance.item_type(item_type_id);
        phase1_instance_builder.add_item_type(instance, item_type_id, item_type.profit, item_type.copies);
    }

    Instance phase1_instance = phase1_instance_builder.build();
    std::cout << "phase 1 start" << std::endl;

    OptimizeParameters phase1_parameters;
    phase1_parameters.verbosity_level = 0;
    phase1_parameters.timer = parameters.timer;
    phase1_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    phase1_parameters.optimization_mode = OptimizationMode::NotAnytime;
    phase1_parameters.not_anytime_tree_search_queue_size = queue_size;
    phase1_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
    phase1_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    phase1_parameters.use_tree_search = true;
    phase1_parameters.json_search_tree_path = "search_tree";
    auto phase1_output = optimize(phase1_instance, phase1_parameters);
    std::cout << "phase 1 end" << std::endl;

    if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
        return false;

    const Solution& phase1_solution = phase1_output.solution_pool.best();
    phase1_solution.write("solution_tmp.json");
    if (phase1_solution.number_of_bins() == 0)
        return false;

    ////////////////////////////////////////////////////////////////////////////
    // Phase 2: fix the large items from phase 1 and solve for small items.
    ////////////////////////////////////////////////////////////////////////////

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

    // Count how many copies of each original bin type were used in phase 1.
    std::vector<BinPos> bin_type_used_copies(instance.number_of_bin_types(), 0);
    for (BinPos bin_pos = 0;
            bin_pos < phase1_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = phase1_solution.bin(bin_pos);
        bin_type_used_copies[solution_bin.bin_type_id] += solution_bin.copies;
    }

    std::vector<BinTypeId> phase2_to_orig_bin_type_ids;
    // Add the bins used in phase 1 with the large items fixed at their positions.
    for (BinPos bin_pos = 0;
            bin_pos < phase1_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = phase1_solution.bin(bin_pos);
        BinTypeId orig_bin_type_id = solution_bin.bin_type_id;
        BinTypeId phase2_bin_type_id = (BinTypeId)phase2_to_orig_bin_type_ids.size();
        phase2_instance_builder.add_bin_type(
                instance,
                orig_bin_type_id,
                solution_bin.copies,
                0);
        phase2_to_orig_bin_type_ids.push_back(orig_bin_type_id);

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

    // Add the remaining copies of each bin type (not used in phase 1) as
    // empty bins, so that small items can be packed into additional bins.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinPos remaining_copies = bin_type.copies - bin_type_used_copies[bin_type_id];
        if (remaining_copies <= 0)
            continue;
        phase2_instance_builder.add_bin_type(
                instance,
                bin_type_id,
                remaining_copies,
                0);
        phase2_to_orig_bin_type_ids.push_back(bin_type_id);
    }

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ItemPos copies = is_in_phase1[item_type_id]?
            item_type.copies_fixed + large_item_placed_copies[item_type_id]:
            item_type.copies;
        phase2_instance_builder.add_item_type(instance, item_type_id, item_type.profit, copies);
    }

    Instance phase2_instance = phase2_instance_builder.build();
    std::cout << "phase 2 start" << std::endl;
    OptimizeParameters phase2_parameters;
    phase2_parameters.verbosity_level = 0;
    phase2_parameters.timer = parameters.timer;
    phase2_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
    phase2_parameters.optimization_mode = OptimizationMode::NotAnytime;
    phase2_parameters.not_anytime_tree_search_queue_size = queue_size;
    phase2_parameters.not_anytime_maximum_approximation_ratio = maximum_approximation_ratio;
    phase2_parameters.linear_programming_solver_name = parameters.linear_programming_solver_name;
    phase2_parameters.use_tree_search = true;
    auto phase2_output = optimize(phase2_instance, phase2_parameters);
    std::cout << "phase 2 end" << std::endl;

    if (phase2_output.solution_pool.best().number_of_bins() == 0)
        return false;

    Solution solution(instance);
    solution.append(
            phase2_output.solution_pool.best(),
            phase2_to_orig_bin_type_ids,
            {});
    std::stringstream ss;
    ss << "LIF q" << queue_size;
    algorithm_formatter.update_solution(solution, ss.str());
    return true;
}

}  // namespace

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

    // Compute is_in_phase1: items included in phase 1.
    // Phase 1 must contain at least min_phase1_items item copies. If the large
    // items alone satisfy this minimum they are the only phase 1 items.
    // Otherwise small items are added in decreasing convex-hull-area order
    // until the minimum is reached (or all items are included).
    const ItemPos min_phase1_items = 16;
    std::vector<bool> is_in_phase1 = is_large;
    ItemPos phase1_total_copies = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (is_large[item_type_id])
            phase1_total_copies += instance.item_type(item_type_id).copies;
    }
    if (phase1_total_copies < min_phase1_items) {
        std::vector<ItemTypeId> small_item_type_ids;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            if (item_type.copies <= item_type.copies_fixed)
                continue;
            if (!is_large[item_type_id])
                small_item_type_ids.push_back(item_type_id);
        }
        std::sort(
                small_item_type_ids.begin(),
                small_item_type_ids.end(),
                [&item_type_convex_hull_areas](ItemTypeId a, ItemTypeId b) {
                    return item_type_convex_hull_areas[a] > item_type_convex_hull_areas[b];
                });
        for (ItemTypeId item_type_id: small_item_type_ids) {
            if (phase1_total_copies >= min_phase1_items)
                break;
            is_in_phase1[item_type_id] = true;
            phase1_total_copies += instance.item_type(item_type_id).copies;
        }
    }

    // Build the phase1 sub-ID → original item type ID mapping (needed inside
    // the helper to interpret phase 1 solution item type IDs).
    std::vector<ItemTypeId> phase1_sub_to_orig_item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!is_in_phase1[item_type_id])
            continue;
        phase1_sub_to_orig_item_type_ids.push_back(item_type_id);
    }

    if (parameters.optimization_mode != OptimizationMode::Anytime) {
        large_item_first_for_queue_size(
                instance,
                is_in_phase1,
                phase1_sub_to_orig_item_type_ids,
                parameters,
                algorithm_formatter,
                parameters.not_anytime_tree_search_queue_size,
                parameters.not_anytime_maximum_approximation_ratio);
    } else {
        NodeId queue_size = 1;
        double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
        for (;;) {
            if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
                break;
            large_item_first_for_queue_size(
                    instance,
                    is_in_phase1,
                    phase1_sub_to_orig_item_type_ids,
                    parameters,
                    algorithm_formatter,
                    queue_size,
                    maximum_approximation_ratio);
            queue_size = std::max(queue_size + 1, (NodeId)(queue_size * 1.5));
            maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;
        }
    }

    algorithm_formatter.end();
    return output;
}
