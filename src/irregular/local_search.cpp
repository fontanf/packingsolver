#include "irregular/local_search.hpp"
#include "irregular/linear_programming.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"

#include "shape/boolean_operations.hpp"
//#include "shape/writer.hpp"

#include <algorithm>
#include <random>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

struct LocalSearchBinData
{
    /** Penalty per item position in this bin. */
    std::vector<Profit> item_penalties;

    /** Lambda (scale factor) per item position in this bin. */
    std::vector<double> lambda;

    /** Remaining area in this bin (heuristic for bin selection). */
    AreaDbl remaining_area = 0;
};

/**
 * Assign a single item to the solution.  Chooses a random orientation and
 * places the item inside the largest free region of the selected bin (boolean
 * difference of the bin AABB minus existing item shapes, defects, and
 * borders).  The bin is the valid one (item AABB fits) with the most remaining
 * area; falls back to the bin with the most remaining area if none fits.
 * Returns the bin position the item was placed in.
 */
BinPos assign_item_to_bin(
        const Instance& instance,
        std::mt19937_64& rng,
        ItemTypeId item_type_id,
        Solution& solution,
        const std::vector<LocalSearchBinData>& bin_data)
{
    const ItemType& item_type = instance.item_type(item_type_id);

    // Choose a random mirror (if allowed).
    std::bernoulli_distribution mirror_dist(0.5);
    const bool mirror = item_type.allow_mirroring && mirror_dist(rng);

    // Choose a random rotation range then a random angle within it.
    std::uniform_int_distribution<Counter> range_dist(
            0, (Counter)item_type.allowed_rotations.size() - 1);
    const auto& rotation_range = item_type.allowed_rotations[range_dist(rng)];
    std::uniform_real_distribution<Angle> angle_dist(
            rotation_range.first, rotation_range.second);
    const Angle angle = angle_dist(rng);

    const AxisAlignedBoundingBox item_aabb = item_type.compute_min_max(angle, mirror);

    // Select the valid bin (item fits at full scale) with the most remaining area.
    // Fall back to the bin with the most remaining area if no bin fits.
    BinPos best_bin_pos = -1;
    AreaDbl best_remaining = -std::numeric_limits<AreaDbl>::infinity();
    for (BinPos bin_pos = 0;
            bin_pos < (BinPos)solution.number_of_different_bins();
            ++bin_pos) {
        const BinTypeId bin_type_id = solution.bin(bin_pos).bin_type_id;
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (bin_type.aabb.x_max - bin_type.aabb.x_min < item_aabb.x_max - item_aabb.x_min
                || bin_type.aabb.y_max - bin_type.aabb.y_min < item_aabb.y_max - item_aabb.y_min)
            continue;
        if (bin_data[bin_pos].remaining_area > best_remaining) {
            best_remaining = bin_data[bin_pos].remaining_area;
            best_bin_pos = bin_pos;
        }
    }
    if (best_bin_pos == -1) {
        best_bin_pos = 0;
        for (BinPos bin_pos = 1;
                bin_pos < (BinPos)solution.number_of_different_bins();
                ++bin_pos) {
            if (bin_data[bin_pos].remaining_area > bin_data[best_bin_pos].remaining_area)
                best_bin_pos = bin_pos;
        }
    }

    // Compute the free space in the bin as the boolean difference between the
    // bin AABB and the shapes of existing items, defects, and borders.
    const BinTypeId best_bin_type_id = solution.bin(best_bin_pos).bin_type_id;
    const BinType& best_bin_type = instance.bin_type(best_bin_type_id);
    const AxisAlignedBoundingBox& bin_aabb = best_bin_type.aabb;

    shape::ShapeWithHoles bin_shape{instance.parameters().scale_value * shape::build_rectangle(bin_aabb), {}};

    std::vector<shape::ShapeWithHoles> occupied_shapes;
    const SolutionBin& best_bin = solution.bin(best_bin_pos);
    for (ItemPos item_pos = 0;
            item_pos < (ItemPos)best_bin.items.size();
            ++item_pos) {
        const ItemType& placed_item_type = instance.item_type(best_bin.items[item_pos].item_type_id);
        for (ItemShapePos shape_pos = 0;
                shape_pos < (ItemShapePos)placed_item_type.shapes.size();
                ++shape_pos) {
            occupied_shapes.push_back(solution.shape_scaled(best_bin_pos, item_pos, shape_pos));
        }
    }
    for (const Defect& defect: best_bin_type.defects)
        occupied_shapes.push_back(defect.shape_scaled);
    for (const Defect& border: best_bin_type.borders)
        occupied_shapes.push_back(border.shape_scaled);

    const std::vector<shape::ShapeWithHoles> free_regions =
            shape::compute_difference(bin_shape, occupied_shapes);
    //shape::Writer().add_shape_with_holes(bin_shape, "Bin")
    //    .add_shapes_with_holes(occupied_shapes, "Occupied")
    //    .add_shapes_with_holes(free_regions, "Difference")
    //    .write_json("difference.json");

    // Select the free region with the largest area.
    Counter best_region_pos = -1;
    AreaDbl best_area = -1.0;
    for (Counter region_pos = 0;
            region_pos < (Counter)free_regions.size();
            ++region_pos) {
        const AreaDbl area = free_regions[region_pos].compute_area();
        if (area > best_area) {
            best_area = area;
            best_region_pos = region_pos;
        }
    }

    // Fall back to the bin centre if no free region was found.
    Point bl_corner;
    if (best_region_pos != -1) {
        bl_corner = free_regions[best_region_pos].find_point_strictly_inside();
    } else {
        bl_corner.x = (instance.parameters().scale_value * bin_aabb.x_min + instance.parameters().scale_value * bin_aabb.x_max) / 2.0;
        bl_corner.y = (instance.parameters().scale_value * bin_aabb.y_min + instance.parameters().scale_value * bin_aabb.y_max) / 2.0;
    }
    //std::cout << "bl_corner " << bl_corner.to_string() << std::endl;
    bl_corner = 1.0 / instance.parameters().scale_value * bl_corner;
    //std::cout << "bl_corner " << bl_corner.to_string() << std::endl;
    solution.add_item(best_bin_pos, item_type_id, bl_corner, angle, mirror);

    return best_bin_pos;
}

/**
 * Assign an item to a bin, then run minimize_shrinkage on that bin until it
 * is feasible (all items at full scale).  Updates solution and bin_data in
 * place.  Returns false if the timer or end-flag fired before feasibility was
 * reached.
 */
bool pack_item(
        const Instance& instance,
        std::mt19937_64& rng,
        ItemTypeId item_type_id,
        Solution& solution,
        std::vector<LocalSearchBinData>& bin_data,
        AlgorithmFormatter& algorithm_formatter,
        const LocalSearchParameters& parameters)
{
    if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
        return false;

    const ItemType& item_type = instance.item_type(item_type_id);

    const BinPos bin_pos = assign_item_to_bin(
            instance, rng, item_type_id, solution, bin_data);

    bin_data[bin_pos].remaining_area -= item_type.area_scaled;
    bin_data[bin_pos].item_penalties.push_back(1.0);
    bin_data[bin_pos].lambda.push_back(0.0);

    Solution bin_solution(instance);
    bin_solution.append(solution, bin_pos, 1);

    bool bin_feasible = false;
    while (!bin_feasible) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            return false;

        LinearProgrammingMinimizeShrinkageParameters lp_params;
        lp_params.timer = parameters.timer;

        const LinearProgrammingMinimizeShrinkageOutput lp_output =
                linear_programming_minimize_shrinkage(
                        bin_solution,
                        bin_data[bin_pos].lambda,
                        bin_data[bin_pos].item_penalties,
                        lp_params);

        bin_feasible = lp_output.feasible;
        bin_data[bin_pos].lambda = lp_output.final_lambda;

        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)lp_output.items_shrunken.size();
                ++item_pos) {
            if (lp_output.items_shrunken[item_pos])
                bin_data[bin_pos].item_penalties[item_pos] += 1.0;
        }

        bin_solution = lp_output.solution;
    }

    // Rebuild solution with the updated bin.
    Solution new_solution(instance);
    for (BinPos bp = 0; bp < solution.number_of_bins(); ++bp) {
        if (bp == bin_pos) {
            new_solution.append(bin_solution, 0, 1);
        } else {
            new_solution.append(solution, bp, 1);
        }
    }
    solution = std::move(new_solution);
    return true;
}

void local_search_bin_packing(
        const Instance& instance,
        const LocalSearchParameters& parameters,
        LocalSearchOutput& output,
        AlgorithmFormatter& algorithm_formatter)
{
    std::mt19937_64 rng(parameters.seed);

    // Open bins: one per item, capped at the available bin count.
    const BinPos number_of_bins = std::min(
            (BinPos)instance.number_of_items(),
            instance.number_of_bins());

    Solution solution(instance);
    for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos)
        solution.add_bin(instance.bin_type_id(bin_pos), 1);

    // Build a flat list of all item copies sorted ascending by area so that
    // pop_back processes the largest items first.
    std::vector<ItemTypeId> unpacked_items;
    unpacked_items.reserve(instance.number_of_items());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies; ++copy)
            unpacked_items.push_back(item_type_id);
    }
    std::sort(unpacked_items.begin(), unpacked_items.end(),
            [&](ItemTypeId a, ItemTypeId b) {
                return instance.item_type(a).area_orig < instance.item_type(b).area_orig;
            });

    // Per-bin state.
    std::vector<LocalSearchBinData> bin_data(number_of_bins);
    for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        bin_data[bin_pos].remaining_area = bin_type.area_scaled;
    }

    for (BinPos target_bins = number_of_bins; target_bins >= 1; --target_bins) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            break;

        // While the solution is incomplete, add one unpacked item to a bin,
        // then immediately run minimize_shrinkage on that bin until it is
        // feasible before moving on to the next item.
        while (!unpacked_items.empty()) {
            if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
                break;
            const ItemTypeId item_type_id = unpacked_items.back();
            unpacked_items.pop_back();
            if (!pack_item(
                    instance,
                    rng,
                    item_type_id,
                    solution,
                    bin_data,
                    algorithm_formatter,
                    parameters))
                break;
        }
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            break;

        // The solution is now complete and all items are at full scale.
        std::stringstream ss;
        ss << "bins " << solution.number_of_bins();
        algorithm_formatter.update_solution(solution, ss.str());

        if (target_bins == 1)
            break;

        // Remove the last bin and move its items to the unpacked list.
        const BinPos last_bin_pos = solution.number_of_bins() - 1;
        const SolutionBin& last_bin = solution.bin(last_bin_pos);

        for (const SolutionItem& solution_item: last_bin.items)
            unpacked_items.push_back(solution_item.item_type_id);
        std::sort(unpacked_items.begin(), unpacked_items.end(),
                [&](ItemTypeId a, ItemTypeId b) {
                    return instance.item_type(a).area_orig < instance.item_type(b).area_orig;
                });

        // Rebuild the solution without the last bin.
        Solution new_solution(instance);
        for (BinPos bin_pos = 0; bin_pos < last_bin_pos; ++bin_pos)
            new_solution.append(solution, bin_pos, 1);
        solution = std::move(new_solution);

        bin_data.resize(last_bin_pos);
    }

    (void)output;
}

void local_search_open_dimension_x(
        const Instance& instance,
        const LocalSearchParameters& parameters,
        LocalSearchOutput& output,
        AlgorithmFormatter& algorithm_formatter)
{
    std::mt19937_64 rng(parameters.seed);

    const BinType& original_bin_type = instance.bin_type(instance.bin_type_id(0));

    // Build a BinPacking sub-instance with a single bin whose shape is the
    // intersection of the original bin shape with a rectangle restricted to
    // x ≤ bin_x_max.  Items are copied in the same order so that item_type_ids
    // are identical in both instances.
    auto make_sub_instance = [&](LengthDbl bin_x_max) -> Instance {
        AxisAlignedBoundingBox restricting_aabb;
        restricting_aabb.x_min = original_bin_type.aabb.x_min;
        restricting_aabb.x_max = bin_x_max;
        restricting_aabb.y_min = original_bin_type.aabb.y_min;
        restricting_aabb.y_max = original_bin_type.aabb.y_max;
        const Shape restricting_rect = shape::build_rectangle(restricting_aabb);

        const std::vector<shape::ShapeWithHoles> intersection = shape::compute_intersection(
                        {{original_bin_type.shape_orig},
                        {restricting_rect}});

        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::BinPacking);
        instance_builder.set_parameters(instance.parameters());
        instance_builder.add_bin_type(intersection[0].shape);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            instance_builder.add_item_type(item_type, item_type.profit, item_type.copies);
        }
        return instance_builder.build();
    };

    // Build a flat list of all item copies sorted ascending by area so that
    // pop_back processes the largest items first.
    std::vector<ItemTypeId> unpacked_items;
    unpacked_items.reserve(instance.number_of_items());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies; ++copy)
            unpacked_items.push_back(item_type_id);
    }
    std::sort(unpacked_items.begin(), unpacked_items.end(),
            [&](ItemTypeId a, ItemTypeId b) {
                return instance.item_type(a).area_orig < instance.item_type(b).area_orig;
            });

    // Pack all items into the original bin (no width restriction yet).
    Solution solution(instance);
    solution.add_bin(instance.bin_type_id(0), 1);

    std::vector<LocalSearchBinData> bin_data(1);
    bin_data[0].remaining_area = original_bin_type.area_scaled;

    while (!unpacked_items.empty()) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            break;
        const ItemTypeId item_type_id = unpacked_items.back();
        unpacked_items.pop_back();
        if (!pack_item(
                instance,
                rng,
                item_type_id,
                solution,
                bin_data,
                algorithm_formatter,
                parameters)) {
            break;
        }

        // Slide all items as far left as possible before measuring the initial width.
        solution = linear_programming_anchor(solution, 1.0, 1.0).solution;
    }
    if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
        return;

    // Record the initial feasible solution.
    LengthDbl best_x_max = solution.x_max();
    {
        std::stringstream ss;
        ss << "x_max " << best_x_max;
        algorithm_formatter.update_solution(solution, ss.str());
    }

    // Iteratively evict the rightmost item and try to fit everything into a
    // sub-instance whose bin is restricted to x ≤ best_x_max.  The bin
    // containment constraints of the LP guarantee new x_max ≤ best_x_max;
    // if strictly smaller we have an improvement.
    for (;;) {
        if (algorithm_formatter.end_boolean() || parameters.timer.needs_to_end())
            break;

        const SolutionBin& bin = solution.bin(0);
        if ((ItemPos)bin.items.size() <= 1)
            break;

        // Build a sub-instance with 95% of the current best width.
        const LengthDbl new_bin_x_max = 0.95 * best_x_max;
        const Instance sub_instance = make_sub_instance(new_bin_x_max);

        // Identify items whose right edge exceeds the new bin width; they must
        // be removed and re-packed.
        std::vector<ItemPos> positions_to_remove;
        for (ItemPos item_pos = 0; item_pos < (ItemPos)bin.items.size(); ++item_pos) {
            const SolutionItem& si = bin.items[item_pos];
            const ItemType& it = instance.item_type(si.item_type_id);
            const AxisAlignedBoundingBox aabb = it.compute_min_max(si.angle, si.mirror);
            if (si.bl_corner.x + aabb.x_max > new_bin_x_max)
                positions_to_remove.push_back(item_pos);
        }

        // Collect item types to re-pack, sorted largest-first.
        std::vector<ItemTypeId> items_to_repack;
        for (ItemPos item_pos : positions_to_remove)
            items_to_repack.push_back(bin.items[item_pos].item_type_id);
        std::sort(items_to_repack.begin(), items_to_repack.end(),
                [&](ItemTypeId a, ItemTypeId b) {
                    return instance.item_type(a).area_orig > instance.item_type(b).area_orig;
                });

        // Build sub-solution with the items that remain inside the new bin.
        Solution sub_solution(sub_instance);
        sub_solution.add_bin(0, 1);
        {
            std::size_t remove_idx = 0;
            for (ItemPos item_pos = 0; item_pos < (ItemPos)bin.items.size(); ++item_pos) {
                if (remove_idx < positions_to_remove.size()
                        && positions_to_remove[remove_idx] == item_pos) {
                    ++remove_idx;
                    continue;
                }
                const SolutionItem& si = bin.items[item_pos];
                sub_solution.add_item(0, si.item_type_id, si.bl_corner, si.angle, si.mirror);
            }
        }

        // Build sub-bin data, carrying over penalties and lambdas for items
        // that remain; erase entries for removed items (highest index first).
        std::vector<LocalSearchBinData> sub_bin_data(1);
        sub_bin_data[0].item_penalties = bin_data[0].item_penalties;
        sub_bin_data[0].lambda = bin_data[0].lambda;
        for (auto it = positions_to_remove.rbegin(); it != positions_to_remove.rend(); ++it) {
            sub_bin_data[0].item_penalties.erase(
                    sub_bin_data[0].item_penalties.begin() + *it);
            sub_bin_data[0].lambda.erase(
                    sub_bin_data[0].lambda.begin() + *it);
        }
        sub_bin_data[0].remaining_area = sub_instance.bin_type(0).area_scaled;
        {
            std::size_t remove_idx = 0;
            for (ItemPos item_pos = 0; item_pos < (ItemPos)bin.items.size(); ++item_pos) {
                if (remove_idx < positions_to_remove.size()
                        && positions_to_remove[remove_idx] == item_pos) {
                    ++remove_idx;
                    continue;
                }
                sub_bin_data[0].remaining_area -=
                        sub_instance.item_type(bin.items[item_pos].item_type_id).area_scaled;
            }
        }

        // Re-pack each item that was outside the new bin.
        bool ok = true;
        for (ItemTypeId item_type_id : items_to_repack) {
            if (!pack_item(
                    sub_instance,
                    rng,
                    item_type_id,
                    sub_solution,
                    sub_bin_data,
                    algorithm_formatter,
                    parameters)) {
                ok = false;
                break;
            }
        }
        if (!ok)
            break;

        // Convert the sub-solution back to the original instance.
        // bin_type_ids: sub bin type 0 → original bin type 0.
        // item_type_ids: identity (items added in the same order).
        Solution new_solution(instance);
        new_solution.append(sub_solution, 0, 1, {instance.bin_type_id(0)}, {});
        solution = std::move(new_solution);
        bin_data[0] = std::move(sub_bin_data[0]);

        // Slide all items as far left as possible before measuring the initial width.
        solution = linear_programming_anchor(solution, 1.0, 1.0).solution;

        // Check for improvement.
        best_x_max = solution.x_max();

        std::stringstream ss;
        ss << "x_max " << solution.x_max();
        algorithm_formatter.update_solution(solution, ss.str());
    }
}

}  // namespace

LocalSearchOutput packingsolver::irregular::local_search(
        const Instance& instance,
        const LocalSearchParameters& parameters)
{
    LocalSearchOutput output(instance);

    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    switch (instance.objective()) {
    case Objective::BinPacking:
    case Objective::BinPackingWithLeftovers:
        local_search_bin_packing(instance, parameters, output, algorithm_formatter);
        break;
    case Objective::OpenDimensionX:
        local_search_open_dimension_x(instance, parameters, output, algorithm_formatter);
        break;
    default:
        throw std::invalid_argument(
                "local_search: unsupported objective "
                + std::to_string(static_cast<int>(instance.objective())));
    }

    algorithm_formatter.end();
    return output;
}
