#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/solution_builder.hpp"

#include "shape/shape.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/shapes_intersections.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Packing of the bounding boxes of the items in columns: a feasible solution,
 * from which the sequential feasibility starts.
 *
 * Each item is packed with the orientation which minimizes the area of its
 * bounding box. The items are sorted by decreasing width; each one is packed
 * in the first column where it fits, else in a new column, else in a new
 * bin. In a column, an item is packed as low as possible: while its bounding
 * box intersects a border, a defect or a fixed item of the bin, it is moved
 * up. The fixed items of the bins used are added to the solution.
 *
 * For the OpenDimensionY objective, x and y are swapped: the items are packed
 * in rows. For the OpenDimensionXY objective, the height of the bin is the
 * aspect ratio times its width.
 */
struct ColumnPacking
{
    /** 'true' iff all the items have been packed. */
    bool feasible = false;

    /**
     * For the OpenDimensionXY objective, width of the bin, in scaled
     * coordinates.
     */
    LengthDbl x_max = 0;

    /** Solution. */
    Solution solution;
};

/**
 * Bounding box in the coordinates of the column packing: the columns follow
 * each other along 'u', and the items of a column along 'v'.
 */
struct UvBox
{
    LengthDbl u_min;
    LengthDbl u_max;
    LengthDbl v_min;
    LengthDbl v_max;
};

/** Return 'true' iff the interiors of two boxes intersect. */
bool intersect(
        const UvBox& box_1,
        const UvBox& box_2)
{
    return shape::strictly_greater(box_1.u_max, box_2.u_min)
        && shape::strictly_greater(box_2.u_max, box_1.u_min)
        && shape::strictly_greater(box_1.v_max, box_2.v_min)
        && shape::strictly_greater(box_2.v_max, box_1.v_min);
}

ColumnPacking pack_columns(const Instance& instance)
{
    const LengthDbl scale_value = instance.parameters().scale_value;
    const LengthDbl ratio = instance.parameters().open_dimension_xy_aspect_ratio;
    const LengthDbl item_item_spacing = instance.item_spacing_scaled();
    const bool rows = (instance.objective() == Objective::OpenDimensionY);
    const bool open_dimension_xy = (instance.objective() == Objective::OpenDimensionXY);
    const LengthDbl infinity = std::numeric_limits<LengthDbl>::infinity();

    auto to_uv = [rows](const AxisAlignedBoundingBox& aabb)
    {
        return (!rows)?
            UvBox{aabb.x_min, aabb.x_max, aabb.y_min, aabb.y_max}:
            UvBox{aabb.y_min, aabb.y_max, aabb.x_min, aabb.x_max};
    };
    auto to_xy = [rows](const UvBox& box)
    {
        AxisAlignedBoundingBox aabb;
        aabb.x_min = (!rows)? box.u_min: box.v_min;
        aabb.x_max = (!rows)? box.u_max: box.v_max;
        aabb.y_min = (!rows)? box.v_min: box.u_min;
        aabb.y_max = (!rows)? box.v_max: box.u_max;
        return aabb;
    };

    // The bounding box of each item, with the orientation which minimizes its
    // area: among the allowed angles, the bounds of the ranges and the
    // multiples of 90 degrees in them.
    struct PackedItem
    {
        ItemTypeId item_type_id;
        Angle angle;
        bool mirror;
        AxisAlignedBoundingBox aabb;
        UvBox box;
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
        best.box = to_uv(best.aabb);
        for (ItemPos copy = item_type.copies_fixed; copy < item_type.copies; ++copy)
            items.push_back(best);
    }
    std::stable_sort(
            items.begin(),
            items.end(),
            [](const PackedItem& item_1, const PackedItem& item_2)
            {
                return item_1.box.u_max - item_1.box.u_min
                    > item_2.box.u_max - item_2.box.u_min;
            });

    // Current bin: its bounding box, the part of it where the items can be
    // packed, and its obstacles: the borders and the defects, inflated, and
    // the bounding boxes of the fixed items, with the item spacing.
    struct Obstacle
    {
        UvBox box;

        /** Shape, or 'nullptr' if it is the box. */
        const ShapeWithHoles* shape = nullptr;
    };
    BinPos number_of_bins = 0;
    BinPos bin_pos = -1;
    LengthDbl bin_spacing = 0;
    UvBox bin_aabb;
    UvBox bin_box;
    std::vector<Obstacle> obstacles;
    struct Column
    {
        LengthDbl u_start;
        LengthDbl width;
        LengthDbl v_end;
    };
    std::vector<Column> columns;
    ColumnPacking output{false, 0, Solution(instance)};
    SolutionBuilder solution_builder(instance);

    // Add the next bin, with its fixed items. Return 'false' if all the bins
    // are already used.
    auto add_bin = [&]()
    {
        if (number_of_bins == instance.number_of_bins())
            return false;
        BinTypeId bin_type_id = instance.bin_type_id(number_of_bins);
        const BinType& bin_type = instance.bin_type(bin_type_id);
        bin_pos = solution_builder.add_bin(bin_type_id, 1);
        number_of_bins++;
        bin_spacing = instance.bin_spacing_scaled(bin_type_id);
        bin_aabb = to_uv(bin_type.aabb_scaled);
        bin_box = {
            bin_aabb.u_min + bin_spacing,
            bin_aabb.u_max - bin_spacing,
            bin_aabb.v_min + bin_spacing,
            bin_aabb.v_max - bin_spacing};
        columns.clear();
        obstacles.clear();
        for (const Defect& border: bin_type.borders) {
            obstacles.push_back({
                    to_uv(border.shape_inflated.shape.compute_min_max()),
                    &border.shape_inflated});
        }
        for (const Defect& defect: bin_type.defects) {
            obstacles.push_back({
                    to_uv(defect.shape_inflated.shape.compute_min_max()),
                    &defect.shape_inflated});
        }
        for (ItemPos fixed_item_pos = 0;
                fixed_item_pos < (ItemPos)bin_type.fixed_items.size();
                ++fixed_item_pos) {
            solution_builder.add_fixed_item(bin_pos, fixed_item_pos);
            const FixedItem& fixed_item = bin_type.fixed_items[fixed_item_pos];
            const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
            AxisAlignedBoundingBox aabb = item_type.compute_min_max(
                    fixed_item.angle,
                    fixed_item.mirror,
                    1);
            aabb.x_min += scale_value * fixed_item.bl_corner.x - item_item_spacing;
            aabb.x_max += scale_value * fixed_item.bl_corner.x + item_item_spacing;
            aabb.y_min += scale_value * fixed_item.bl_corner.y - item_item_spacing;
            aabb.y_max += scale_value * fixed_item.bl_corner.y + item_item_spacing;
            obstacles.push_back({to_uv(aabb), nullptr});
        }
        return true;
    };

    // Return the lowest position, not lower than 'v', of the box from 'u_min'
    // to 'u_max' of height 'height' which doesn't intersect an obstacle and
    // ends below 'v_max'; infinity if there is none. The box is moved above
    // the bounding boxes of the obstacles it intersects, but only by a quarter
    // of its height for the obstacles which aren't boxes, which can leave
    // space next to them, like the borders of a circular bin.
    auto lowest_position = [&obstacles, &to_xy, infinity](
            LengthDbl u_min,
            LengthDbl u_max,
            LengthDbl height,
            LengthDbl v,
            LengthDbl v_max)
    {
        for (;;) {
            if (shape::strictly_greater(v + height, v_max))
                return infinity;
            UvBox box = {u_min, u_max, v, v + height};
            Shape rectangle = shape::build_rectangle(to_xy(box));
            LengthDbl v_next = v;
            for (const Obstacle& obstacle: obstacles) {
                if (!intersect(obstacle.box, box))
                    continue;
                if (obstacle.shape == nullptr) {
                    v_next = (std::max)(v_next, obstacle.box.v_max);
                } else if (shape::intersect(*obstacle.shape, rectangle, true)) {
                    v_next = (std::max)(v_next, (std::min)(v + height / 4, obstacle.box.v_max));
                }
            }
            if (v_next == v)
                return v;
            v = v_next;
        }
    };

    if (!add_bin())
        return output;
    for (const PackedItem& item: items) {
        LengthDbl width = item.box.u_max - item.box.u_min;
        LengthDbl height = item.box.v_max - item.box.v_min;
        // The first column where the item fits (it fits in width, since the
        // items are sorted by decreasing width).
        LengthDbl v_max = bin_box.v_max;
        if (open_dimension_xy) {
            v_max = (std::min)(
                    v_max,
                    bin_aabb.v_min + ratio * output.x_max - bin_spacing);
        }
        Counter column_id = -1;
        LengthDbl v = infinity;
        for (Counter c = 0; c < (Counter)columns.size(); ++c) {
            v = lowest_position(
                    columns[c].u_start,
                    columns[c].u_start + width,
                    height,
                    columns[c].v_end + item_item_spacing,
                    v_max);
            if (v != infinity) {
                column_id = c;
                break;
            }
        }
        if (column_id == -1) {
            // A new column, after the last one. If the obstacles prevent
            // packing the item in it, it is moved right, as an item is moved
            // up in a column; if the bin is full, the item is packed in a new
            // bin.
            LengthDbl u_start = (columns.empty())?
                bin_box.u_min:
                columns.back().u_start + columns.back().width + item_item_spacing;
            for (;;) {
                if (!shape::strictly_greater(u_start + width, bin_box.u_max)) {
                    UvBox column_box = {u_start, u_start + width, bin_box.v_min, bin_box.v_max};
                    v = lowest_position(
                            column_box.u_min,
                            column_box.u_max,
                            height,
                            column_box.v_min,
                            column_box.v_max);
                    if (v != infinity)
                        break;
                    LengthDbl u_next = infinity;
                    for (const Obstacle& obstacle: obstacles) {
                        if (!intersect(obstacle.box, column_box))
                            continue;
                        u_next = (std::min)(
                                u_next,
                                (obstacle.shape == nullptr)?
                                obstacle.box.u_max:
                                (std::min)(u_start + width / 4, obstacle.box.u_max));
                    }
                    if (u_next != infinity) {
                        u_start = u_next;
                        continue;
                    }
                }
                if (!add_bin())
                    return output;
                u_start = bin_box.u_min;
            }
            columns.push_back({u_start, width, 0});
            column_id = columns.size() - 1;
        }
        Column& column = columns[column_id];
        column.v_end = v + height;
        if (open_dimension_xy) {
            output.x_max = (std::max)(
                    output.x_max,
                    column.u_start + column.width + bin_spacing - bin_aabb.u_min);
            output.x_max = (std::max)(
                    output.x_max,
                    (column.v_end + bin_spacing - bin_aabb.v_min) / ratio);
        }
        AxisAlignedBoundingBox aabb = to_xy({column.u_start, column.u_start + width, v, v + height});
        Point bl_corner;
        bl_corner.x = (aabb.x_min - item.aabb.x_min) / scale_value;
        bl_corner.y = (aabb.y_min - item.aabb.y_min) / scale_value;
        solution_builder.add_item(bin_pos, item.item_type_id, bl_corner, item.angle, item.mirror);
    }
    // The bins up to the last one with fixed items must be used.
    while (number_of_bins <= instance.last_bin_with_fixed_items())
        add_bin();
    output.solution = solution_builder.build();
    output.feasible = true;
    return output;
}

}

namespace
{

/**
 * Return true iff the fixed items of a bin type stay inside the bin once
 * restricted to 'restricting_aabb' (taking the item-bin spacing into
 * account).
 */
bool fixed_items_inside(
        const Instance& instance,
        BinTypeId bin_type_id,
        const AxisAlignedBoundingBox& restricting_aabb)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const LengthDbl scale_value = instance.parameters().scale_value;
    const LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
    for (const FixedItem& fixed_item: bin_type.fixed_items) {
        const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
        AxisAlignedBoundingBox aabb = item_type.compute_min_max(
                fixed_item.angle,
                fixed_item.mirror,
                1);
        if (shape::strictly_greater(
                    scale_value * fixed_item.bl_corner.x + aabb.x_max,
                    restricting_aabb.x_max - bin_spacing)
                || shape::strictly_greater(
                    scale_value * fixed_item.bl_corner.y + aabb.y_max,
                    restricting_aabb.y_max - bin_spacing)) {
            return false;
        }
    }
    return true;
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

    // Last solution found, the warm start of the next sub-problem.
    Solution previous_solution(instance);
    bool has_previous_solution = false;

    // Initialize the open dimension variable(s) and/or bin count.
    BinPos current_number_of_bins = 0;
    // The bins up to the last one with fixed items must be used: a
    // sub-problem with fewer bins would turn the fixed items of the missing
    // bins into free items.
    const BinPos minimum_number_of_bins = (std::max)((BinPos)1, instance.last_bin_with_fixed_items() + 1);
    LengthDbl x = 0;
    LengthDbl y = 0;

    // Without the column packing, the first iteration uses all the bins, at
    // their full size.
    {
        current_number_of_bins = instance.number_of_bins();
        const BinType& last_bin_type = instance.bin_type(
                instance.bin_type_id(current_number_of_bins - 1));
        x = last_bin_type.aabb_scaled.x_max - last_bin_type.aabb_scaled.x_min;
        y = last_bin_type.aabb_scaled.y_max - last_bin_type.aabb_scaled.y_min;
        if (instance.objective() == Objective::OpenDimensionXY)
            y = x * instance.parameters().open_dimension_xy_aspect_ratio;
    }

    // Update the open dimension variable(s) and/or bin count from a solution
    // found, shrinking them by 'shrink_ratio'. Return false if there is
    // nothing left to try.
    const double shrink_ratio = parameters.shrink_ratio;
    auto update_targets = [&instance, &current_number_of_bins, &x, &y, shrink_ratio, minimum_number_of_bins](
            const Solution& solution)
    {
        if (instance.objective() == Objective::BinPacking) {
            current_number_of_bins = solution.number_of_bins() - 1;
            if (current_number_of_bins < minimum_number_of_bins)
                return false;
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            BinTypeId bin_type_id_last = instance.bin_type_id(solution.number_of_bins() - 1);
            const BinType& bin_type_last = instance.bin_type(bin_type_id_last);
            LengthDbl scale_value = instance.parameters().scale_value;
            x = (1 - shrink_ratio) * (scale_value * solution.x_max() - bin_type_last.aabb_scaled.x_min)
                + scale_value * bin_type_last.item_bin_minimum_spacing;
            if (solution.number_of_bins() < current_number_of_bins) {
                current_number_of_bins = solution.number_of_bins();
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                x = bin_type_new.aabb_scaled.x_max;
            } else if (!strictly_greater(x, bin_type_last.aabb_scaled.x_min + scale_value * bin_type_last.item_bin_minimum_spacing)) {
                current_number_of_bins = solution.number_of_bins() - 1;
                if (current_number_of_bins < minimum_number_of_bins)
                    return false;
                BinTypeId bin_type_id_new = instance.bin_type_id(current_number_of_bins - 1);
                const BinType& bin_type_new = instance.bin_type(bin_type_id_new);
                x = bin_type_new.aabb_scaled.x_max;
            }
        } else if (instance.objective() == Objective::OpenDimensionX) {
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            LengthDbl scale_value = instance.parameters().scale_value;
            x = (1 - shrink_ratio) * (scale_value * solution.x_max() - bin_type.aabb_scaled.x_min)
                + scale_value * bin_type.item_bin_minimum_spacing;
        } else if (instance.objective() == Objective::OpenDimensionY) {
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            LengthDbl scale_value = instance.parameters().scale_value;
            y = (1 - shrink_ratio) * (scale_value * solution.y_max() - bin_type.aabb_scaled.y_min)
                + scale_value * bin_type.item_bin_minimum_spacing;
        } else {  // OpenDimensionXY
            const BinType& bin_type = instance.bin_type(instance.bin_type_id(0));
            LengthDbl scale_value = instance.parameters().scale_value;
            // The bin must shrink at each iteration, else the same solution
            // can be found again and again.
            x = (1 - shrink_ratio) * x;
            x = (std::min)(x, (1 - shrink_ratio) * (std::max)(
                    scale_value * solution.x_max() - bin_type.aabb_scaled.x_min,
                    scale_value * solution.y_max() - bin_type.aabb_scaled.y_min)
                + scale_value * bin_type.item_bin_minimum_spacing);
            AreaDbl a_cur = (solution.x_max() - solution.x_min()) * (solution.y_max() - solution.y_min());
            LengthDbl x_cur = scale_value * std::sqrt(a_cur / instance.parameters().open_dimension_xy_aspect_ratio);
            if (x > x_cur)
                x = x_cur;
            y = x * instance.parameters().open_dimension_xy_aspect_ratio;
        }
        return true;
    };

    // A packing of the items in columns: a first solution, from which the
    // targets of the first iteration are shrunk. It is the warm start of the
    // first iteration only for the OpenDimensionXY objective: otherwise, the
    // local search, which would start from it, is slower to compact the items
    // than when it starts from scratch.
    bool stop = false;
    ColumnPacking column_packing = pack_columns(instance);
    if (column_packing.feasible) {
        algorithm_formatter.update_solution(column_packing.solution, "SF columns");
        if (instance.objective() == Objective::OpenDimensionXY) {
            previous_solution = column_packing.solution;
            has_previous_solution = true;
            x = (1 - parameters.shrink_ratio) * column_packing.x_max;
            y = x * instance.parameters().open_dimension_xy_aspect_ratio;
        } else {
            current_number_of_bins = column_packing.solution.number_of_bins();
            stop = !update_targets(column_packing.solution);
        }
    }

    for (Counter it = 0; !stop; ++it) {
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        // Build the Feasibility sub-instance.
        InstanceBuilder sub_instance_builder;
        sub_instance_builder.set_objective(Objective::Feasibility);
        sub_instance_builder.set_parameters(instance.parameters());
        sub_instance_builder.set_original_instance(instance);
        std::vector<BinTypeId> sub_to_orig_bin_type_ids;
        bool fixed_item_outside = false;
        if (instance.objective() == Objective::BinPacking) {
            // Add bin types from the original instance up to current_number_of_bins.
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type.copies, remaining_bins);
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
                remaining_bins -= copies;
            }
        } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
            // Like BinPacking, but the last bin is x-restricted (like
            // OpenDimensionX). The other bins keep their bin type. The last
            // bin is a bin type of its own, added after the others, unless it
            // is the only copy of its bin type.
            BinPos remaining_bins = current_number_of_bins;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < instance.number_of_bin_types() && remaining_bins > 0;
                    ++bin_type_id) {
                const BinType& bin_type = instance.bin_type(bin_type_id);
                BinPos copies = std::min(bin_type.copies, remaining_bins);
                remaining_bins -= copies;
                BinPos copies_full = (remaining_bins == 0)? copies - 1: copies;
                if (copies_full > 0) {
                    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, copies_full);
                    sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                    sub_to_orig_bin_type_ids.push_back(bin_type_id);
                }
                if (remaining_bins > 0)
                    continue;

                // Last bin.
                AxisAlignedBoundingBox restricting_aabb = bin_type.aabb_scaled;
                restricting_aabb.x_max = restricting_aabb.x_min + x;
                if (!fixed_items_inside(instance, bin_type_id, restricting_aabb)) {
                    fixed_item_outside = true;
                    break;
                }
                BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
                sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
                sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
                sub_instance_builder.crop_bin_type(
                        sub_bin_type_id,
                        x / instance.parameters().scale_value,
                        bin_type.aabb_orig.y_max - bin_type.aabb_orig.y_min);
                sub_to_orig_bin_type_ids.push_back(bin_type_id);
            }
        } else {
            // Build a single bin as the intersection of the original bin with a
            // rectangle restricted to the current open dimension estimate.
            BinTypeId bin_type_id = instance.bin_type_id(0);
            AxisAlignedBoundingBox restricting_aabb = instance.bin_type(bin_type_id).aabb_scaled;
            restricting_aabb.x_max = restricting_aabb.x_min + x;
            restricting_aabb.y_max = restricting_aabb.y_min + y;
            if (!fixed_items_inside(instance, bin_type_id, restricting_aabb))
                fixed_item_outside = true;
            BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
            sub_instance_builder.set_bin_type_copies_min(sub_bin_type_id, 0);
            sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 1);
            sub_instance_builder.crop_bin_type(
                    sub_bin_type_id,
                    x / instance.parameters().scale_value,
                    y / instance.parameters().scale_value);
        }
        // A fixed item would be out of the restricted bin: the objective can't
        // be improved anymore.
        if (fixed_item_outside)
            break;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            sub_instance_builder.add_item_type_from_original_instance(item_type_id);
        }
        Instance sub_instance = sub_instance_builder.build();

        // Solve the sub-instance, warm-started from the previous solution.
        SolutionPool<Instance, Solution> sub_solution_pool = solver(
                sub_instance,
                (has_previous_solution)? &previous_solution: nullptr);

        // No feasible solution found: stop.
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
        ss << "SF it " << it;
        if (!sub_solution_pool.best_label().empty())
            ss << " " << sub_solution_pool.best_label();
        algorithm_formatter.update_solution(solution, ss.str());
        previous_solution = solution;
        has_previous_solution = true;

        // Update for the next iteration.
        if (!update_targets(solution))
            break;
    }

    algorithm_formatter.end();
    return output;
}
