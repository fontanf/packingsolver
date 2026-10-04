#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/solution_builder.hpp"

#include "shape/shape.hpp"
#include "shape/boolean_operations.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Packing of the bounding boxes of the items in columns, for the
 * OpenDimensionXY objective: a feasible solution, whose bin gives the
 * initial size of the open dimensions.
 *
 * Each item is packed with the orientation which minimizes the area of its
 * bounding box. The items are sorted by decreasing width; each one is packed
 * in the first column where it fits, else in a new column. The height of the
 * bin is always the aspect ratio times its width.
 */
struct ColumnPacking
{
    /** Width of the bin, in scaled coordinates. */
    LengthDbl x_max = 0;

    /** Height of the bin, in scaled coordinates. */
    LengthDbl y_max = 0;

    /** Solution. */
    Solution solution;
};

ColumnPacking pack_columns(const Instance& instance)
{
    const LengthDbl scale_value = instance.parameters().scale_value;
    const LengthDbl ratio = instance.parameters().open_dimension_xy_aspect_ratio;
    const BinTypeId bin_type_id = instance.bin_type_id(0);
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const LengthDbl item_item_spacing = scale_value * instance.parameters().item_item_minimum_spacing;
    const LengthDbl item_bin_spacing = scale_value * bin_type.item_bin_minimum_spacing;

    // The bounding box of each item, with the orientation which minimizes its
    // area: among the allowed angles, the bounds of the ranges and the
    // multiples of 90 degrees in them.
    struct PackedItem
    {
        ItemTypeId item_type_id;
        Angle angle;
        bool mirror;
        AxisAlignedBoundingBox aabb;
    };
    std::vector<PackedItem> items;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        PackedItem best;
        best.item_type_id = item_type_id;
        AreaDbl best_area = std::numeric_limits<AreaDbl>::infinity();
        for (const AllowedRotation& rotation: item_type.allowed_rotations) {
            std::vector<Angle> angles = {rotation.start_angle, rotation.end_angle};
            for (Angle angle = std::ceil(rotation.start_angle / 90) * 90;
                    angle <= rotation.end_angle;
                    angle += 90) {
                angles.push_back(angle);
            }
            for (Angle angle: angles) {
                AxisAlignedBoundingBox aabb = item_type.compute_min_max(angle, rotation.mirror, 1);
                AreaDbl area = (aabb.x_max - aabb.x_min) * (aabb.y_max - aabb.y_min);
                if (area < best_area) {
                    best_area = area;
                    best.angle = angle;
                    best.mirror = rotation.mirror;
                    best.aabb = aabb;
                }
            }
        }
        for (ItemPos copy = 0; copy < item_type.copies; ++copy)
            items.push_back(best);
    }
    std::stable_sort(
            items.begin(),
            items.end(),
            [](const PackedItem& item_1, const PackedItem& item_2)
            {
                return item_1.aabb.x_max - item_1.aabb.x_min
                    > item_2.aabb.x_max - item_2.aabb.x_min;
            });

    // Pack the items in columns, relatively to the bottom-left corner of the
    // bin.
    struct Column
    {
        LengthDbl x_start;
        LengthDbl width;
        LengthDbl y_end;
    };
    std::vector<Column> columns;
    ColumnPacking output{0, 0, Solution(instance)};
    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
    for (const PackedItem& item: items) {
        LengthDbl width = item.aabb.x_max - item.aabb.x_min;
        LengthDbl height = item.aabb.y_max - item.aabb.y_min;
        // The first column where the item fits (it fits in width, since the
        // items are sorted by decreasing width).
        Counter column_id = -1;
        for (Counter c = 0; c < (Counter)columns.size(); ++c) {
            if (columns[c].y_end + item_item_spacing + height + item_bin_spacing
                    <= output.y_max) {
                column_id = c;
                break;
            }
        }
        LengthDbl y_start = 0;
        if (column_id != -1) {
            y_start = columns[column_id].y_end + item_item_spacing;
        } else {
            // A new column. The width of the bin must contain it, and its
            // height, the item.
            Column column;
            column.x_start = (columns.empty())?
                item_bin_spacing:
                columns.back().x_start + columns.back().width + item_item_spacing;
            column.width = width;
            columns.push_back(column);
            column_id = columns.size() - 1;
            y_start = item_bin_spacing;
            output.x_max = (std::max)(output.x_max, column.x_start + width + item_bin_spacing);
            output.x_max = (std::max)(output.x_max, (y_start + height + item_bin_spacing) / ratio);
            output.y_max = ratio * output.x_max;
        }
        columns[column_id].y_end = y_start + height;
        Point bl_corner;
        bl_corner.x = (bin_type.aabb_scaled.x_min + columns[column_id].x_start - item.aabb.x_min) / scale_value;
        bl_corner.y = (bin_type.aabb_scaled.y_min + y_start - item.aabb.y_min) / scale_value;
        solution_builder.add_item(bin_pos, item.item_type_id, bl_corner, item.angle, item.mirror);
    }
    output.solution = solution_builder.build();
    return output;
}

}

SequentialFeasibilityOutput packingsolver::irregular::sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters)
{
    SequentialFeasibilityOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    // Compute total item AABB area to derive the initial bin size, in scaled
    // coordinates, as the bins.
    AreaDbl total_item_aabb_area = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        AxisAlignedBoundingBox aabb = item_type.compute_min_max(0.0, false, 1);
        LengthDbl dx = aabb.x_max - aabb.x_min;
        LengthDbl dy = aabb.y_max - aabb.y_min;
        total_item_aabb_area += dx * dy * item_type.copies;
    }

    // Initialize the open dimension variable(s) and/or bin count.
    BinPos current_number_of_bins = 0;
    LengthDbl x = 0;
    LengthDbl y = 0;
    if (instance.objective() == Objective::BinPacking) {
        AreaDbl total_bin_aabb_area = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            LengthDbl bx = bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min;
            LengthDbl by = bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min;
            AreaDbl bin_aabb_area = bx * by;
            AreaDbl remaining = 2.0 * total_item_aabb_area - total_bin_aabb_area;
            BinPos copies_needed = (BinPos)std::ceil(remaining / bin_aabb_area);
            BinPos copies_used = std::min(copies_needed, bin_type.copies);
            total_bin_aabb_area += copies_used * bin_aabb_area;
            current_number_of_bins += copies_used;
            if (total_bin_aabb_area >= 2.0 * total_item_aabb_area)
                break;
        }
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        AreaDbl total_bin_aabb_area = 0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            LengthDbl bx = bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min;
            LengthDbl by = bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min;
            AreaDbl bin_aabb_area = bx * by;
            AreaDbl remaining = 2.0 * total_item_aabb_area - total_bin_aabb_area;
            BinPos copies_needed = (BinPos)std::ceil(remaining / bin_aabb_area);
            BinPos copies_used = std::min(copies_needed, bin_type.copies);
            total_bin_aabb_area += copies_used * bin_aabb_area;
            current_number_of_bins += copies_used;
            if (total_bin_aabb_area >= 2.0 * total_item_aabb_area)
                break;
        }
        BinTypeId last_bin_type_id = instance.bin_type_id(current_number_of_bins - 1);
        const BinType& last_bin_type = instance.bin_type(last_bin_type_id);
        x = last_bin_type.aabb_scaled.x_max - last_bin_type.aabb_scaled.x_min;
    } else if (instance.objective() == Objective::OpenDimensionX) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
        y = bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min;
        x = 2 * total_item_aabb_area / y;
    } else if (instance.objective() == Objective::OpenDimensionY) {
        const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
        x = bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min;
        y = 2 * total_item_aabb_area / x;
    } else {  // OpenDimensionXY
        // A packing of the items in columns: a first solution, and the bin
        // of the first iteration is smaller.
        ColumnPacking column_packing = pack_columns(instance);
        algorithm_formatter.update_solution(column_packing.solution, "SF columns");
        x = 0.99 * column_packing.x_max;
        y = x * instance.parameters().open_dimension_xy_aspect_ratio;
    }

    for (Counter it = 0;; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        if (instance.objective() == Objective::BinPacking) {
            // Add bin types from the original instance up to current_number_of_bins.
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            // Like BinPacking but each bin type is x-restricted (like OpenDimensionX).
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type.copies, remaining_bins);
                AxisAlignedBoundingBox restricting_aabb = bin_type.aabb_scaled;
                restricting_aabb.x_max = restricting_aabb.x_min + x;
                const Shape restricting_rect = shape::build_rectangle(restricting_aabb);
                // Intersect in scaled coordinates (both operands scaled), then
                // convert the result back to original (unscaled) units: the
                // sub-instance builder re-applies scale_value to shape_orig
                // during build(), so passing an already-scaled shape here
                // would scale it down twice.
                const shape::MultiShapeWithHoles intersection = shape::compute_intersection(
                        {{bin_type.shape_scaled},
                        {restricting_rect}});
                Shape sub_bin_shape_orig = (1.0 / instance.parameters().scale_value)
                    * intersection.shapes_with_holes[0].shape;
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(
                        sub_bin_shape_orig);
                sub_instance_builder.set_bin_type_cost(sub_bin_type_id, bin_type.cost);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_instance_builder.set_item_bin_minimum_spacing(
                        sub_bin_type_id, bin_type.item_bin_minimum_spacing);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
                remaining_bins -= copies;
            }
        } else {
            // Build a single bin as the intersection of the original bin with a
            // rectangle restricted to the current open dimension estimate.
            const BinType& original_bin_type = instance.bin_type(instance.bin_type_id(0));
            AxisAlignedBoundingBox restricting_aabb = original_bin_type.aabb_scaled;
            restricting_aabb.x_max = restricting_aabb.x_min + x;
            restricting_aabb.y_max = restricting_aabb.y_min + y;
            const Shape restricting_rect = shape::build_rectangle(restricting_aabb);
            // Intersect in scaled coordinates, then convert back to original
            // (unscaled) units before handing off to the sub-instance builder,
            // which re-applies scale_value during build().
            const shape::MultiShapeWithHoles intersection = shape::compute_intersection(
                    {{original_bin_type.shape_scaled},
                    {restricting_rect}});
            Shape sub_bin_shape_orig = (1.0 / instance.parameters().scale_value)
                * intersection.shapes_with_holes[0].shape;
            sub_instance_builder.add_bin_type(sub_bin_shape_orig);
            sub_instance_builder.set_item_bin_minimum_spacing(
                    0,
                    instance.bin_type(0).item_bin_minimum_spacing);
        }
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            sub_instance_builder.add_item_type(instance, item_type_id);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(sub_instance);

        // If no feasible solution found, stop.
        if (!sub_solution_pool.best().feasible())
            break;

        // Transfer the sub-solution to the main instance.
        Solution solution(instance);
        if (instance.objective() == Objective::BinPacking
                || instance.objective() == Objective::BinPackingWithLeftovers) {
            solution.append_bins(
                    sub_solution_pool.best(),
                    sub_to_orig_bin_type_ids,
                    {});
        } else {
            solution.append_bin(
                    sub_solution_pool.best(),
                    0,  // bin_pos
                    1,  // copies
                    {0});  // bin_type_ids
        }
        std::stringstream ss;
        ss << "SF it " << it << " " << sub_solution_pool.best_label();
        algorithm_formatter.update_solution(solution, ss.str());

        // Update for the next iteration.
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins == 0)
                break;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            BinTypeId bin_type_id_last = instance.bin_type_id(solution.number_of_bins() - 1);
            const BinType& bin_type_last = instance.bin_type(bin_type_id_last);
            LengthDbl scale_value = instance.parameters().scale_value;
            x = 0.99 * (scale_value * solution.x_max() - bin_type_last.aabb_scaled.x_min)
                + scale_value * bin_type_last.item_bin_minimum_spacing;
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                x = bin_type_new.aabb_scaled.x_max;
            } else if (!strictly_greater(x, bin_type_last.aabb_scaled.x_min + scale_value * bin_type_last.item_bin_minimum_spacing)) {
                current_number_of_bins = solution.number_of_bins() - 1;
                if (current_number_of_bins == 0)
                    break;
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                x = bin_type_new.aabb_scaled.x_max;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            LengthDbl scale_value = instance.parameters().scale_value;
            x = 0.99 * (scale_value * solution.x_max() - bin_type.aabb_scaled.x_min)
                + scale_value * bin_type.item_bin_minimum_spacing;
        } else if (instance.objective() == Objective::OpenDimensionY) {
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            LengthDbl scale_value = instance.parameters().scale_value;
            y = 0.99 * (scale_value * solution.y_max() - bin_type.aabb_scaled.y_min)
                + scale_value * bin_type.item_bin_minimum_spacing;
        } else {  // OpenDimensionXY
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            LengthDbl scale_value = instance.parameters().scale_value;
            // The bin must shrink at each iteration, else the same solution
            // can be found again and again.
            x = 0.99 * x;
            x = (std::min)(x, 0.99 * (std::max)(
                    scale_value * solution.x_max() - bin_type.aabb_scaled.x_min,
                    scale_value * solution.y_max() - bin_type.aabb_scaled.y_min)
                + scale_value * bin_type.item_bin_minimum_spacing);
            AreaDbl a_cur = (solution.x_max() - solution.x_min()) * (solution.y_max() - solution.y_min());
            LengthDbl x_cur = scale_value * std::sqrt(a_cur / instance.parameters().open_dimension_xy_aspect_ratio);
            if (x > x_cur)
                x = x_cur;
            y = x * instance.parameters().open_dimension_xy_aspect_ratio;
        }
    }

    algorithm_formatter.end();
    return output;
}
