#include "irregular/local_search.hpp"
#include "irregular/linear_programming.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

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

    // Choose a random allowed rotation entry, then a random angle within it.
    std::uniform_int_distribution<Counter> range_dist(
            0, (Counter)item_type.allowed_rotations.size() - 1);
    const AllowedRotation& rotation_range = item_type.allowed_rotations[range_dist(rng)];
    std::uniform_real_distribution<Angle> angle_dist(
            rotation_range.start_angle, rotation_range.end_angle);
    const Angle angle = angle_dist(rng);
    const bool mirror = rotation_range.mirror;

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
        if (bin_type.aabb_orig.x_max - bin_type.aabb_orig.x_min < item_aabb.x_max - item_aabb.x_min
                || bin_type.aabb_orig.y_max - bin_type.aabb_orig.y_min < item_aabb.y_max - item_aabb.y_min)
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
    const AxisAlignedBoundingBox& bin_aabb = best_bin_type.aabb_orig;

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

}  // namespace

LocalSearchOutput packingsolver::irregular::local_search(
        const Instance& instance,
        const LocalSearchParameters& parameters)
{
    LocalSearchOutput output(instance);

    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    std::mt19937_64 rng(parameters.seed);

    Solution solution(instance);
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos)
        solution.add_bin(instance.bin_type_id(bin_pos), 1);

    // Pre-place fixed items and initialize their bin_data entries.
    // Per-bin state.
    std::vector<LocalSearchBinData> bin_data(instance.number_of_bins());
    for (BinPos bin_pos = 0; bin_pos < instance.number_of_bins(); ++bin_pos) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(bin_pos));
        bin_data[bin_pos].remaining_area = bin_type.area_scaled;
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
            solution.add_item(
                    bin_pos,
                    fixed_item.item_type_id,
                    fixed_item.bl_corner,
                    fixed_item.angle,
                    fixed_item.mirror,
                    true);
            bin_data[bin_pos].remaining_area -= item_type.area_scaled;
            bin_data[bin_pos].item_penalties.push_back(
                    std::numeric_limits<Profit>::infinity());
            bin_data[bin_pos].lambda.push_back(1.0);
        }
    }

    // Build a flat list of all remaining item copies sorted ascending by area
    // so that pop_back processes the largest items first.
    // Exclude copies already fixed in the bin types.
    std::vector<ItemTypeId> unpacked_items;
    unpacked_items.reserve(instance.number_of_items());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies - item_type.copies_fixed; ++copy)
            unpacked_items.push_back(item_type_id);
    }
    std::sort(unpacked_items.begin(), unpacked_items.end(),
            [&](ItemTypeId a, ItemTypeId b) {
                return instance.item_type(a).area_orig < instance.item_type(b).area_orig;
            });

    // Pack all items into the available bins.
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

    std::stringstream ss;
    ss << "LS";
    algorithm_formatter.update_solution(solution, ss.str());

    algorithm_formatter.end();
    return output;
}
