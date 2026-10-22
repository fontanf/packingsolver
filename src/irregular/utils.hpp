/**
 * Private helpers shared between the algorithms of the irregular problem.
 *
 * All functions are inline to allow inclusion from multiple translation units.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "shape/basic_shapes.hpp"
#include "shape/convex_partition.hpp"
#include "shape/intersection_tree.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace packingsolver
{
namespace irregular
{

/** Apply mirror and rotation to a BasicShape (both the shape and circle center). */
inline BasicShape apply_placement(
        const BasicShape& basic_shape,
        bool mirror,
        Angle angle)
{
    BasicShape result = basic_shape;
    if (mirror) {
        result.shape = result.shape.axial_symmetry_y_axis();
        if (result.type == BasicShapeType::CircularSegment
                || result.type == BasicShapeType::Circle) {
            result.circle_center = result.circle_center.axial_symmetry_y_axis();
        }
    }
    result.shape = result.shape.rotate(angle);
    if (result.type == BasicShapeType::CircularSegment
            || result.type == BasicShapeType::Circle) {
        result.circle_center = result.circle_center.rotate(angle);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Convex decomposition of the instance
////////////////////////////////////////////////////////////////////////////////

struct InstanceBasicShapeDecomposition
{
    std::vector<std::vector<std::vector<BasicShape>>> item_types;
    std::vector<std::vector<BasicShape>> bin_types_borders;
    std::vector<std::vector<BasicShape>> bin_types_defects;
};

inline InstanceBasicShapeDecomposition compute_instance_basic_shape_decomposition(
        const Instance& instance)
{
    InstanceBasicShapeDecomposition ibsd;

    ibsd.item_types.resize(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        ibsd.item_types[item_type_id].resize(item_type.shapes.size());
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const BasicShapesDecomposition decomp = decompose_into_basic_shapes(
                    item_type.shapes[item_shape_pos].shape_scaled);
            for (const BasicShape& basic_shape: decomp.basic_shapes)
                ibsd.item_types[item_type_id][item_shape_pos].push_back(basic_shape);
        }
    }

    ibsd.bin_types_borders.resize(instance.number_of_bin_types());
    ibsd.bin_types_defects.resize(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            for (const Shape& part: shape::compute_convex_partition(
                    bin_type.borders[border_pos].shape_scaled)) {
                BasicShape basic_shape;
                basic_shape.type = BasicShapeType::ConvexPolygon;
                basic_shape.shape = part;
                ibsd.bin_types_borders[bin_type_id].push_back(basic_shape);
            }
        }
        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            for (const Shape& part: shape::compute_convex_partition(
                    bin_type.defects[defect_pos].shape_scaled)) {
                BasicShape basic_shape;
                basic_shape.type = BasicShapeType::ConvexPolygon;
                basic_shape.shape = part;
                ibsd.bin_types_defects[bin_type_id].push_back(basic_shape);
            }
        }
    }

    return ibsd;
}

////////////////////////////////////////////////////////////////////////////////
// Potentially intersecting part pairs
////////////////////////////////////////////////////////////////////////////////

struct IntersectingItemPartDefectPart
{
    ItemPos item_var_pos = -1;
    ItemShapePos item_shape_pos = -1;
    ItemShapePos item_part_pos = -1;
    DefectId defect_pos = -1;
    ItemShapePos defect_part_pos = -1;
};

struct IntersectingItemParts
{
    ItemPos item_1_var_pos = -1;
    ItemShapePos item_shape_1_pos = -1;
    ItemShapePos item_part_1_pos = -1;
    ItemPos item_2_var_pos = -1;
    ItemShapePos item_shape_2_pos = -1;
    ItemShapePos item_part_2_pos = -1;
};

struct IntersectingParts
{
    std::vector<IntersectingItemPartDefectPart> intersecting_item_part_border_part;

    std::vector<IntersectingItemPartDefectPart> intersecting_item_part_defect_part;

    std::vector<IntersectingItemParts> intersecting_item_part_fixed_item_part;

    std::vector<IntersectingItemParts> intersecting_item_parts;

    ItemShapePos number_of_intersecting_parts() const
    {
        return this->intersecting_item_part_border_part.size()
                + this->intersecting_item_part_defect_part.size()
                + this->intersecting_item_part_fixed_item_part.size()
                + this->intersecting_item_parts.size();
    }
};

////////////////////////////////////////////////////////////////////////////////
// Per-item bounding boxes
////////////////////////////////////////////////////////////////////////////////

/**
 * Per-item bounding boxes used by the LP.
 *
 * For each level (item = union over all parts, part = per convex part):
 *
 * - *_aabbs: local (scaled instance) space — shape extents before translation.
 *   Used as constraint coefficients.
 *
 * - *_solution_aabbs: world position at current bl_corner and lambda.
 *   Equals bl_corner_scaled + current_lambda * local_aabb.
 *
 * - *_movement_aabbs: solution AABB expanded by movement_box_half_size.
 *   Used as RHS for bin containment constraints (before clamping to bin).
 *   part_movement_aabbs is also passed to compute_potentially_intersecting_parts.
 */
struct ItemAxisAlignedBoundingBoxes
{
    // Item-level (union over all parts and shapes).
    AxisAlignedBoundingBox item_aabb;
    AxisAlignedBoundingBox item_solution_aabb;
    AxisAlignedBoundingBox item_movement_aabb;
    // Per-shape, per-convex-part.
    std::vector<std::vector<AxisAlignedBoundingBox>> part_aabbs;
    std::vector<std::vector<AxisAlignedBoundingBox>> part_solution_aabbs;
    std::vector<std::vector<AxisAlignedBoundingBox>> part_movement_aabbs;
};

/**
 * Compute item bounding boxes for each item in the bin.
 */
inline std::vector<ItemAxisAlignedBoundingBoxes> compute_unfixed_item_aabbs(
        const Instance& instance,
        const SolutionBin& solution_bin,
        LengthDbl movement_box_half_size,
        const std::vector<std::vector<std::vector<BasicShape>>>& item_rotated_decompositions,
        const std::vector<double>& current_lambda)
{
    std::vector<ItemAxisAlignedBoundingBoxes> item_aabbs;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
        const SolutionItem& solution_item = solution_bin.items[item_pos];
        if (solution_item.is_fixed)
            continue;
        const LengthDbl cx = solution_bin.items[item_pos].bl_corner.x * instance.parameters().scale_value;
        const LengthDbl cy = solution_bin.items[item_pos].bl_corner.y * instance.parameters().scale_value;
        const double lam = current_lambda[item_pos];

        ItemAxisAlignedBoundingBoxes aabbs;
        aabbs.part_aabbs.resize(item_rotated_decompositions[item_pos].size());
        aabbs.part_solution_aabbs.resize(item_rotated_decompositions[item_pos].size());
        aabbs.part_movement_aabbs.resize(item_rotated_decompositions[item_pos].size());
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_rotated_decompositions[item_pos].size();
                ++item_shape_pos) {
            const auto& parts = item_rotated_decompositions[item_pos][item_shape_pos];
            aabbs.part_aabbs[item_shape_pos].resize(parts.size());
            aabbs.part_solution_aabbs[item_shape_pos].resize(parts.size());
            aabbs.part_movement_aabbs[item_shape_pos].resize(parts.size());
            for (Counter part_pos = 0; part_pos < (Counter)parts.size(); ++part_pos) {
                const AxisAlignedBoundingBox p = parts[part_pos].shape.compute_min_max();

                AxisAlignedBoundingBox ps;
                ps.x_min = cx + lam * p.x_min;
                ps.x_max = cx + lam * p.x_max;
                ps.y_min = cy + lam * p.y_min;
                ps.y_max = cy + lam * p.y_max;

                AxisAlignedBoundingBox pm = ps;
                pm.x_min = ps.x_min - movement_box_half_size;
                pm.x_max = ps.x_max + movement_box_half_size;
                pm.y_min = ps.y_min - movement_box_half_size;
                pm.y_max = ps.y_max + movement_box_half_size;

                aabbs.part_aabbs[item_shape_pos][part_pos] = p;
                aabbs.part_solution_aabbs[item_shape_pos][part_pos] = ps;
                aabbs.part_movement_aabbs[item_shape_pos][part_pos] = pm;

                aabbs.item_aabb = merge(aabbs.item_aabb, p);
                aabbs.item_solution_aabb = merge(aabbs.item_solution_aabb, ps);
                aabbs.item_movement_aabb = merge(aabbs.item_movement_aabb, pm);
            }
        }
        item_aabbs.push_back(aabbs);
    }
    return item_aabbs;
}

enum ShapeType { Item, FixedItem, Border, Defect };

struct TreeShapeInfo
{
    ShapeType shape_type;
    ItemPos item_id;
    ItemShapePos item_shape_pos;
    Counter part_pos;
};

/**
 * Compute the convex-part pairs that pass the AABB intersection test,
 * given pre-computed world AABBs (shifted + expanded) for each item part.
 *
 * Uses a single shape::IntersectionTree over all border, defect, and item
 * part AABBs. Each tree shape is tracked via TreeShapeInfo (ShapeType +
 * item_id/item_shape_pos/part_pos) so pair classification needs no offset
 * arithmetic.
 */
inline IntersectingParts compute_potentially_intersecting_parts(
        const std::vector<ItemAxisAlignedBoundingBoxes>& item_movement_aabbs,
        const std::vector<AxisAlignedBoundingBox>& fixed_item_part_aabbs,
        const std::vector<AxisAlignedBoundingBox>& border_part_aabbs,
        const std::vector<AxisAlignedBoundingBox>& defect_part_aabbs)
{
    // Build the unified tree and index map: borders, then defects, then items.
    // For Border/Defect shapes, item_id holds the obstacle index.
    std::vector<TreeShapeInfo> shape_index_map;
    std::vector<shape::ShapeWithHoles> all_shapes;

    for (Counter fixed_item_part_pos = 0;
            fixed_item_part_pos < (Counter)fixed_item_part_aabbs.size();
            ++fixed_item_part_pos) {
        shape_index_map.push_back({ShapeType::FixedItem, 0, 0, fixed_item_part_pos});
        shape::ShapeWithHoles swh;
        swh.shape = shape::build_rectangle(fixed_item_part_aabbs[fixed_item_part_pos]);
        all_shapes.push_back(std::move(swh));
    }
    for (Counter border_part_pos = 0;
            border_part_pos < (Counter)border_part_aabbs.size();
            ++border_part_pos) {
        shape_index_map.push_back({ShapeType::Border, 0, 0, border_part_pos});
        shape::ShapeWithHoles swh;
        swh.shape = shape::build_rectangle(border_part_aabbs[border_part_pos]);
        all_shapes.push_back(std::move(swh));
    }
    for (Counter defect_part_pos = 0;
            defect_part_pos < (Counter)defect_part_aabbs.size();
            ++defect_part_pos) {
        shape_index_map.push_back({ShapeType::Defect, 0, 0, defect_part_pos});
        shape::ShapeWithHoles swh;
        swh.shape = shape::build_rectangle(defect_part_aabbs[defect_part_pos]);
        all_shapes.push_back(std::move(swh));
    }
    for (ItemPos item_var_pos = 0;
            item_var_pos < (ItemPos)item_movement_aabbs.size();
            ++item_var_pos) {
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_movement_aabbs[item_var_pos].part_movement_aabbs.size();
                ++item_shape_pos) {
            for (Counter part_pos = 0;
                    part_pos < (Counter)item_movement_aabbs[item_var_pos].part_movement_aabbs[item_shape_pos].size();
                    ++part_pos) {
                shape_index_map.push_back({ShapeType::Item, item_var_pos, item_shape_pos, part_pos});
                shape::ShapeWithHoles swh;
                swh.shape = shape::build_rectangle(
                        item_movement_aabbs[item_var_pos].part_movement_aabbs[item_shape_pos][part_pos]);
                all_shapes.push_back(std::move(swh));
            }
        }
    }

    shape::IntersectionTree tree(all_shapes, {}, {});
    std::vector<std::pair<shape::ShapePos, shape::ShapePos>> pairs =
            tree.compute_intersecting_shapes(false);

    IntersectingParts output;
    for (Counter pair_pos = 0; pair_pos < (Counter)pairs.size(); ++pair_pos) {
        TreeShapeInfo shape_info_1 = shape_index_map[pairs[pair_pos].first];
        TreeShapeInfo shape_info_2 = shape_index_map[pairs[pair_pos].second];

        // Skip pairs with no item involved.
        if (shape_info_1.shape_type != ShapeType::Item && shape_info_2.shape_type != ShapeType::Item)
            continue;

        // Ensure shape_info_1 is always the item.
        if (shape_info_2.shape_type == ShapeType::Item)
            std::swap(shape_info_1, shape_info_2);

        if (shape_info_2.shape_type == ShapeType::FixedItem) {
            output.intersecting_item_part_fixed_item_part.push_back({
                    shape_info_1.item_id,
                    shape_info_1.item_shape_pos,
                    (ItemShapePos)shape_info_1.part_pos,
                    shape_info_2.item_id,
                    shape_info_2.item_shape_pos,
                    (ItemShapePos)shape_info_2.part_pos});
        } else if (shape_info_2.shape_type == ShapeType::Border) {
            output.intersecting_item_part_border_part.push_back({
                    shape_info_1.item_id,
                    shape_info_1.item_shape_pos,
                    (ItemShapePos)shape_info_1.part_pos,
                    (DefectId)shape_info_2.part_pos,
                    0});
        } else if (shape_info_2.shape_type == ShapeType::Defect) {
            output.intersecting_item_part_defect_part.push_back({
                    shape_info_1.item_id,
                    shape_info_1.item_shape_pos,
                    (ItemShapePos)shape_info_1.part_pos,
                    (DefectId)shape_info_2.part_pos,
                    0});
        } else {
            // Both are items.
            if (shape_info_1.item_id == shape_info_2.item_id)
                continue;
            if (shape_info_1.item_id > shape_info_2.item_id)
                std::swap(shape_info_1, shape_info_2);
            output.intersecting_item_parts.push_back({
                    shape_info_1.item_id,
                    shape_info_1.item_shape_pos,
                    (ItemShapePos)shape_info_1.part_pos,
                    shape_info_2.item_id,
                    shape_info_2.item_shape_pos,
                    (ItemShapePos)shape_info_2.part_pos});
        }
    }
    return output;
}

struct EdgeSeparationConstraintParameters
{
    /** Shape to which the separating line belongs. */
    ShapePos edge_shape_pos = -1;

    /** Separating element. */
    ElementPos edge_element_pos = -1;

    /** Element of the closest point. */
    ElementPos point_element_pos = -1;

    /** Closest point from the separating element. */
    Point point;

    /** Signed distance between the closest element and the separating line. */
    LengthDbl distance;

    /**
     * Signed distance with the unscaled point.
     *
     * Used as tie-breaker when the shape scale is 0.0.
     */
    LengthDbl distance_unscaled;


    LengthDbl coef_x1;
    LengthDbl coef_y1;
    LengthDbl coef_x2;
    LengthDbl coef_y2;
    LengthDbl coef_lambda1;
    LengthDbl coef_lambda2;
};

/** Compute the separation constraint of an edge of a shape and another shape. */
inline EdgeSeparationConstraintParameters compute_edge_separation_constraint_parameters(
        const Shape& shape_from,
        const Point& shift_from,
        double scale_from,
        ElementPos element_from_pos,
        const Shape& shape_to,
        const Point& shift_to,
        double scale_to)
{
    EdgeSeparationConstraintParameters output;
    const ShapeElement& element_from = shape_from.elements[element_from_pos];
    output.distance = std::numeric_limits<LengthDbl>::infinity();
    for (ElementPos element_to_pos = 0;
            element_to_pos < (ElementPos)shape_to.elements.size();
            ++element_to_pos) {
        const ShapeElement& element_to = shape_to.elements[element_to_pos];
        const Point& point = element_to.start;
        LengthDbl distance = signed_distance_point_to_line(
                scale_to * point + shift_to,
                scale_from * element_from.start + shift_from,
                scale_from * element_from.start + shift_from + (element_from.end - element_from.start));
        LengthDbl distance_unscaled = signed_distance_point_to_line(
                point + shift_to,
                scale_from * element_from.start + shift_from,
                scale_from * element_from.start + shift_from + (element_from.end - element_from.start));
        if (shape::strictly_lesser(distance, output.distance)
                || (shape::equal(distance, output.distance)
                    && shape::strictly_lesser(distance_unscaled, output.distance_unscaled))) {
            output.distance = distance;
            output.distance_unscaled = distance_unscaled;
            output.point_element_pos = element_to_pos;
            output.point = point;
        }
    }

    // Original points: (X11, Y11) (X12, Y12)
    //
    // Shifted and shrunken points: (x1 + l1 * X11, y1 + l1 * Y11) (x1 + l1 * X12, y1 + l1 * Y12)
    // A = l1 * (Y12 - Y11)
    // B = l1 * (X11 - X12)
    // C = (x1 + l1 * X12) * (y1 + l1 * Y11) - (x1 + l1 * X11) * (y1 + l1 * Y12
    // C = l1 * (x1 * (Y11 - Y12) + y1 * (X12 - X11) + l1 * (X12 Y11 - X11 Y12))
    //
    // Simplify by l1:
    // A = Y12 - Y11
    // B = X11 - X12
    // C = x1 * (Y11 - Y12) + y1 * (X12 - X11) + l1 * (X12 Y11 - X11 Y12)
    //
    // Original point: (X2, Y2)
    // Shifted and shrunken point: (x2 + l2 * X2, y2 + l2 * Y2)
    // Final constraint:
    //     A * x2
    //   + B * y2
    //   + (A * X2 + B * Y2) * l2
    //   - A * x1
    //   - B * y1
    //   + (X12 Y11 - X11 Y12) * l1 >= 0
    LengthDbl a = element_from.end.y - element_from.start.y;
    LengthDbl b = element_from.start.x - element_from.end.x;
    output.coef_x1 = -a;
    output.coef_y1 = -b;
    output.coef_x2 = a;
    output.coef_y2 = b;
    output.coef_lambda1 = element_from.end.x * element_from.start.y
        - element_from.start.x * element_from.end.y;
    output.coef_lambda2 = a * output.point.x + b * output.point.y;
    //std::cout << "a " << a << " output.point.x " << output.point.x
    //    << " prod " << a * output.point.x
    //    << " b " << b << " output.point.y " << output.point.y
    //    << " prod " << b * output.point.y << std::endl;
    return output;
}

/**
 * Find an edge to separate two given shapes.
 *
 * The selected edge is the less constraining one.
 */
inline EdgeSeparationConstraintParameters find_best_edge_separator(
        const Shape& shape_1,
        const Point& shift_1,
        double scale_1,
        const Shape& shape_2,
        const Point& shift_2,
        double scale_2)
{
    //std::cout << "compute_edge_separation_constraint_parameters"
    //    << " shape_1 " << shape_1.elements.size()
    //    << " shift_1 " << shift_1.to_string()
    //    << " scale_1 " << scale_1
    //    << " shape_2 " << shape_2.elements.size()
    //    << " shift_2 " << shift_2.to_string()
    //    << " scale_2 " << scale_2
    //    << std::endl;
    EdgeSeparationConstraintParameters output;
    output.distance = -std::numeric_limits<LengthDbl>::infinity();

    for (ElementPos element_1_pos = 0;
            element_1_pos < (ElementPos)shape_1.elements.size();
            ++element_1_pos) {
        const ShapeElement& element = shape_1.elements[element_1_pos];
        EdgeSeparationConstraintParameters p = compute_edge_separation_constraint_parameters(
                shape_1,
                shift_1,
                scale_1,
                element_1_pos,
                shape_2,
                shift_2,
                scale_2);
        if (output.distance < p.distance) {
            output = p;
            output.edge_shape_pos = 0;
            output.edge_element_pos = element_1_pos;
        }
    }

    for (ElementPos element_2_pos = 0;
            element_2_pos < (ElementPos)shape_2.elements.size();
            ++element_2_pos) {
        const ShapeElement& element = shape_2.elements[element_2_pos];
        EdgeSeparationConstraintParameters p = compute_edge_separation_constraint_parameters(
                shape_2,
                shift_2,
                scale_2,
                element_2_pos,
                shape_1,
                shift_1,
                scale_1);
        if (output.distance < p.distance) {
            std::swap(p.coef_x1, p.coef_x2);
            std::swap(p.coef_y1, p.coef_y2);
            std::swap(p.coef_lambda1, p.coef_lambda2);
            output = p;
            output.edge_shape_pos = 1;
            output.edge_element_pos = element_2_pos;
        }
    }

    LengthDbl value
        = output.coef_x1 * shift_1.x
        + output.coef_y1 * shift_1.y
        + output.coef_x2 * shift_2.x
        + output.coef_y2 * shift_2.y
        + output.coef_lambda1 * scale_1
        + output.coef_lambda2 * scale_2;
    //if (shape::strictly_lesser(value, 0.0)) {
    if (shape::strictly_lesser(value, -1e-6)) {
        const ShapeElement& edge_element = ((output.edge_shape_pos == 0)?
                    shape_1.elements[output.edge_element_pos]:
                    shape_2.elements[output.edge_element_pos]);
        const ShapeElement& point_element = ((output.edge_shape_pos == 0)?
                    shape_2.elements[output.point_element_pos]:
                    shape_1.elements[output.point_element_pos]);
        throw std::logic_error(
                FUNC_SIGNATURE + ": violated separation constraint; "
                "edge_element: " + edge_element.to_string() + "; "
                "point_element: " + point_element.to_string() + "; "
                "point: " + output.point.to_string() + "; "
                "distance: " + std::to_string(output.distance) + "; "
                "coef_x1: " + std::to_string(output.coef_x1) + "; "
                "coef_y1: " + std::to_string(output.coef_y1) + "; "
                "coef_lambda1: " + std::to_string(output.coef_lambda1) + "; "
                "coef_x2: " + std::to_string(output.coef_x2) + "; "
                "coef_y2: " + std::to_string(output.coef_y2) + "; "
                "coef_lambda2: " + std::to_string(output.coef_lambda2) + "; "
                "value: " + std::to_string(value) + ".");
    }

    return output;
}

}  // namespace irregular
}  // namespace packingsolver
