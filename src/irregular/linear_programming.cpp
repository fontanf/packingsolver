#include "irregular/linear_programming.hpp"

#include "shape/convex_partition.hpp"
#include "shape/intersection_tree.hpp"
#include "shape/writer.hpp"

#include "mathoptsolverscmake/milp.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

struct InstanceConvexDecomposition
{
    std::vector<std::vector<std::vector<Shape>>> item_types;
    std::vector<std::vector<Shape>> bin_types_borders;
    std::vector<std::vector<Shape>> bin_types_defects;
};

InstanceConvexDecomposition compute_instance_convex_decomposition(
        const Instance& instance)
{
    InstanceConvexDecomposition icd;

    icd.item_types.resize(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        icd.item_types[item_type_id].resize(item_type.shapes.size());
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            icd.item_types[item_type_id][item_shape_pos] = shape::compute_convex_partition(
                    item_type.shapes[item_shape_pos].shape_scaled);
        }
    }

    icd.bin_types_borders.resize(instance.number_of_bin_types());
    icd.bin_types_defects.resize(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (Counter border_pos = 0;
                border_pos < (Counter)bin_type.borders.size();
                ++border_pos) {
            for (const Shape& part: shape::compute_convex_partition(
                    bin_type.borders[border_pos].shape_scaled))
                icd.bin_types_borders[bin_type_id].push_back(part);
        }
        for (Counter defect_pos = 0;
                defect_pos < (Counter)bin_type.defects.size();
                ++defect_pos) {
            for (const Shape& part: shape::compute_convex_partition(
                    bin_type.defects[defect_pos].shape_scaled))
                icd.bin_types_defects[bin_type_id].push_back(part);
        }
    }

    return icd;
}

EdgeSeparationConstraintParameters compute_edge_separation_constraint_parameters(
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

}

EdgeSeparationConstraintParameters packingsolver::irregular::find_best_edge_separator(
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
        std::cout << "shift_1 " << shift_1.to_string()
            << " shift_2 " << shift_2.to_string() << std::endl;
        std::cout << "scale_1 " << scale_1
            << " scale_2 " << scale_2
            << std::endl;
        Shape shape_1_shifted = shape_1;
        Shape shape_2_shifted = shape_2;
        shape_1_shifted.shift(shift_1.x, shift_1.y);
        shape_2_shifted.shift(shift_2.x, shift_2.y);
        Shape shape_1_scaled = scale_1 * shape_1;
        Shape shape_2_scaled = scale_2 * shape_2;
        shape_1_scaled.shift(shift_1.x, shift_1.y);
        shape_2_scaled.shift(shift_2.x, shift_2.y);
        Writer()
            .add_shape(shape_1)
            .add_shape(shape_1_scaled)
            .add_shape(shape_2)
            .add_shape(shape_2_scaled)
            .write_json("tmp.json");
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

namespace
{

struct IntersectingItemPartDefectPart
{
    ItemPos item_pos = -1;
    ItemShapePos item_shape_pos = -1;
    ItemShapePos item_part_pos = -1;
    DefectId defect_pos = -1;
    ItemShapePos defect_part_pos = -1;
};

struct IntersectingItemParts
{
    ItemPos item_1_pos = -1;
    ItemShapePos item_shape_1_pos = -1;
    ItemShapePos item_part_1_pos = -1;
    ItemPos item_2_pos = -1;
    ItemShapePos item_shape_2_pos = -1;
    ItemShapePos item_part_2_pos = -1;
};

struct IntersectingParts
{
    std::vector<IntersectingItemPartDefectPart> intersecting_item_part_border_part;

    std::vector<IntersectingItemPartDefectPart> intersecting_item_part_defect_part;

    std::vector<IntersectingItemParts> intersecting_item_parts;

    ItemShapePos number_of_intersecting_parts() const
    {
        return intersecting_item_part_border_part.size()
                + intersecting_item_part_defect_part.size()
                + intersecting_item_parts.size();
    }
};

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
std::vector<ItemAxisAlignedBoundingBoxes> compute_item_aabbs(
        const Instance& instance,
        const SolutionBin& solution_bin,
        LengthDbl movement_box_half_size,
        const std::vector<std::vector<std::vector<Shape>>>& item_rotated_decompositions,
        const std::vector<double>& current_lambda)
{
    std::vector<ItemAxisAlignedBoundingBoxes> item_aabbs(solution_bin.items.size());
    for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
        const LengthDbl cx = solution_bin.items[item_pos].bl_corner.x * instance.parameters().scale_value;
        const LengthDbl cy = solution_bin.items[item_pos].bl_corner.y * instance.parameters().scale_value;
        const double lam = current_lambda[item_pos];

        const Counter n_shapes = item_rotated_decompositions[item_pos].size();
        item_aabbs[item_pos].part_aabbs.resize(n_shapes);
        item_aabbs[item_pos].part_solution_aabbs.resize(n_shapes);
        item_aabbs[item_pos].part_movement_aabbs.resize(n_shapes);
        for (ItemShapePos item_shape_pos = 0; item_shape_pos < n_shapes; ++item_shape_pos) {
            const auto& parts = item_rotated_decompositions[item_pos][item_shape_pos];
            item_aabbs[item_pos].part_aabbs[item_shape_pos].resize(parts.size());
            item_aabbs[item_pos].part_solution_aabbs[item_shape_pos].resize(parts.size());
            item_aabbs[item_pos].part_movement_aabbs[item_shape_pos].resize(parts.size());
            for (Counter part_pos = 0; part_pos < (Counter)parts.size(); ++part_pos) {
                const AxisAlignedBoundingBox p = parts[part_pos].compute_min_max();

                AxisAlignedBoundingBox ps;
                ps.x_min = cx + lam * p.x_min;
                ps.x_max = cx + lam * p.x_max;
                ps.y_min = cy + lam * p.y_min;
                ps.y_max = cy + lam * p.y_max;

                AxisAlignedBoundingBox pm;
                pm.x_min = ps.x_min - movement_box_half_size;
                pm.x_max = ps.x_max + movement_box_half_size;
                pm.y_min = ps.y_min - movement_box_half_size;
                pm.y_max = ps.y_max + movement_box_half_size;

                item_aabbs[item_pos].part_aabbs[item_shape_pos][part_pos] = p;
                item_aabbs[item_pos].part_solution_aabbs[item_shape_pos][part_pos] = ps;
                item_aabbs[item_pos].part_movement_aabbs[item_shape_pos][part_pos] = pm;

                item_aabbs[item_pos].item_aabb = merge(item_aabbs[item_pos].item_aabb, p);
                item_aabbs[item_pos].item_solution_aabb = merge(item_aabbs[item_pos].item_solution_aabb, ps);
                item_aabbs[item_pos].item_movement_aabb = merge(item_aabbs[item_pos].item_movement_aabb, pm);
            }
        }
    }
    return item_aabbs;
}


enum ShapeType { Item, Border, Defect };

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
IntersectingParts compute_potentially_intersecting_parts(
        const std::vector<ItemAxisAlignedBoundingBoxes>& item_movement_aabbs,
        const std::vector<AxisAlignedBoundingBox>& border_part_aabbs,
        const std::vector<AxisAlignedBoundingBox>& defect_part_aabbs)
{
    // Build the unified tree and index map: borders, then defects, then items.
    // For Border/Defect shapes, item_id holds the obstacle index.
    std::vector<TreeShapeInfo> shape_index_map;
    std::vector<shape::ShapeWithHoles> all_shapes;

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
    for (ItemPos item_pos = 0; item_pos < (ItemPos)item_movement_aabbs.size(); ++item_pos) {
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_movement_aabbs[item_pos].part_movement_aabbs.size();
                ++item_shape_pos) {
            for (Counter part_pos = 0;
                    part_pos < (Counter)item_movement_aabbs[item_pos].part_movement_aabbs[item_shape_pos].size();
                    ++part_pos) {
                shape_index_map.push_back({ShapeType::Item, item_pos, item_shape_pos, part_pos});
                shape::ShapeWithHoles swh;
                swh.shape = shape::build_rectangle(
                        item_movement_aabbs[item_pos].part_movement_aabbs[item_shape_pos][part_pos]);
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

        if (shape_info_2.shape_type == ShapeType::Border) {
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

Solution linear_programming_anchor(
        const Instance& instance,
        const Solution& initial_solution,
        double x_weight,
        double y_weight,
        const LinearProgrammingAnchorParameters& parameters,
        BinPos bin_pos,
        const InstanceConvexDecomposition& icd)
{
    Solution solution(instance);
    solution.append(initial_solution, bin_pos, 1);
    double current_value = 0;
    for (const SolutionItem& solution_item: initial_solution.bin(bin_pos).items) {
        current_value += x_weight
                * solution_item.bl_corner.x * instance.parameters().scale_value;
        current_value += y_weight
                * solution_item.bl_corner.y * instance.parameters().scale_value;
    }

    // Get bin type.
    const BinTypeId bin_type_id = solution.bin(0).bin_type_id;
    const BinType& bin_type = instance.bin_type(bin_type_id);

    // Precompute AABBs for border and defect convex parts (fixed world coordinates).
    std::vector<AxisAlignedBoundingBox> border_part_aabbs;
    for (const Shape& part: icd.bin_types_borders[bin_type_id])
        border_part_aabbs.push_back(part.compute_min_max());
    std::vector<AxisAlignedBoundingBox> defect_part_aabbs;
    for (const Shape& part: icd.bin_types_defects[bin_type_id])
        defect_part_aabbs.push_back(part.compute_min_max());

    LengthDbl movement_box_half_size = 10;

    for (Counter number_of_iterations = 0;; ++number_of_iterations) {
        const SolutionBin& solution_bin = solution.bin(0);

        // Precompute rotated convex decompositions for each solution item.
        // item_rotated_decompositions[item_pos][item_shape_pos] = list of convex parts
        // with the item's angle and mirror already applied.
        std::vector<std::vector<std::vector<Shape>>> item_rotated_decompositions(solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(solution_item.item_type_id);
            item_rotated_decompositions[item_pos].resize(item_type.shapes.size());
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const Shape& convex_part: icd.item_types[solution_item.item_type_id][item_shape_pos]) {
                    Shape rotated = convex_part;
                    if (solution_item.mirror)
                        rotated = rotated.axial_symmetry_y_axis();
                    rotated = rotated.rotate(solution_item.angle);
                    item_rotated_decompositions[item_pos][item_shape_pos].push_back(rotated);
                }
            }
        }

        // Lambda is fixed at 1 for anchor_to_corner (no shrinkage variables).
        const std::vector<double> anchor_lambda(solution_bin.items.size(), 1.0);
        const std::vector<ItemAxisAlignedBoundingBoxes> item_aabbs = compute_item_aabbs(
                instance,
                solution_bin,
                movement_box_half_size,
                item_rotated_decompositions,
                anchor_lambda);

        // Precompute per-item bin bounds (without movement restriction).
        // The local bounding box of each item is read from item_aabbs.
        std::vector<AxisAlignedBoundingBox> item_bin_bounds(solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const AxisAlignedBoundingBox& ia = item_aabbs[item_pos].item_aabb;
            item_bin_bounds[item_pos].x_min = bin_type.aabb.x_min * instance.parameters().scale_value - ia.x_min;
            item_bin_bounds[item_pos].x_max = bin_type.aabb.x_max * instance.parameters().scale_value - ia.x_max;
            item_bin_bounds[item_pos].y_min = bin_type.aabb.y_min * instance.parameters().scale_value - ia.y_min;
            item_bin_bounds[item_pos].y_max = bin_type.aabb.y_max * instance.parameters().scale_value - ia.y_max;
        }

        IntersectingParts intersecting_parts = compute_potentially_intersecting_parts(
                item_aabbs,
                border_part_aabbs,
                defect_part_aabbs);

        ////////////////////
        // Setup LP model //
        ////////////////////

        // Build LP model.
        mathoptsolverscmake::MilpModel lp_model;

        /////////////////////////////
        // Objective and variables //
        /////////////////////////////

        // Set objective sense.
        lp_model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;

        std::vector<int> x(solution_bin.items.size());
        std::vector<int> y(solution_bin.items.size());
        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            x[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(std::max(
                    item_bin_bounds[item_pos].x_min,
                    solution_item.bl_corner.x * instance.parameters().scale_value - movement_box_half_size));
            lp_model.variables_upper_bounds.push_back(std::min(
                    item_bin_bounds[item_pos].x_max,
                    solution_item.bl_corner.x * instance.parameters().scale_value +movement_box_half_size));
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(x_weight);
            lp_model.variables_names.push_back("x_" + std::to_string(item_pos));
            y[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(std::max(
                    item_bin_bounds[item_pos].y_min,
                    solution_item.bl_corner.y * instance.parameters().scale_value - movement_box_half_size));
            lp_model.variables_upper_bounds.push_back(std::min(
                    item_bin_bounds[item_pos].y_max,
                    solution_item.bl_corner.y * instance.parameters().scale_value + movement_box_half_size));
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(y_weight);
            lp_model.variables_names.push_back("y_" + std::to_string(item_pos));
        }

        /////////////////
        // Constraints //
        /////////////////

        // Obstacle (shape_1) is at world origin with scale=1 (fixed).
        // Item (shape_2) has LP variables x, y at scale=1 (no lambda in atc).
        // Full constraint: coef_x1*0 + coef_y1*0 + coef_lambda1 + coef_x2*x + coef_y2*y + coef_lambda2 >= 0
        //  => coef_x2*x + coef_y2*y >= -(coef_lambda1 + coef_lambda2)

        const LengthDbl sv = instance.parameters().scale_value;

        // Intersections between items and bin borders.
        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_border_part) {
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    icd.bin_types_borders[bin_type_id][e.defect_pos],
                    Point{0, 0},
                    1.0,
                    item_rotated_decompositions[e.item_pos][e.item_shape_pos][e.item_part_pos],
                    Point{solution_bin.items[e.item_pos].bl_corner.x * sv, solution_bin.items[e.item_pos].bl_corner.y * sv},
                    1.0);
            if (shape::equal(p.coef_x2, 0.0) && shape::equal(p.coef_y2, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "border_i" + std::to_string(e.item_pos)
                    + "_s" + std::to_string(e.item_shape_pos)
                    + "_p" + std::to_string(e.item_part_pos)
                    + "_bp" + std::to_string(e.defect_pos));
            lp_model.constraints_lower_bounds.push_back(-(p.coef_lambda1 + p.coef_lambda2));
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
        }

        // Intersections between items and bin defects.
        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_defect_part) {
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    icd.bin_types_defects[bin_type_id][e.defect_pos],
                    Point{0, 0},
                    1.0,
                    item_rotated_decompositions[e.item_pos][e.item_shape_pos][e.item_part_pos],
                    Point{solution_bin.items[e.item_pos].bl_corner.x * sv, solution_bin.items[e.item_pos].bl_corner.y * sv},
                    1.0);
            if (shape::equal(p.coef_x2, 0.0) && shape::equal(p.coef_y2, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "defect_i" + std::to_string(e.item_pos)
                    + "_s" + std::to_string(e.item_shape_pos)
                    + "_p" + std::to_string(e.item_part_pos)
                    + "_dp" + std::to_string(e.defect_pos));
            lp_model.constraints_lower_bounds.push_back(-(p.coef_lambda1 + p.coef_lambda2));
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
        }

        // Intersections between pairs of items.
        // Full constraint: coef_x1*x1 + coef_y1*y1 + coef_lambda1 + coef_x2*x2 + coef_y2*y2 + coef_lambda2 >= 0
        //  => coef_x1*x1 + coef_y1*y1 + coef_x2*x2 + coef_y2*y2 >= -(coef_lambda1 + coef_lambda2)
        for (const IntersectingItemParts& e: intersecting_parts.intersecting_item_parts) {
            const Shape& convex_part_1 = item_rotated_decompositions[e.item_1_pos][e.item_shape_1_pos][e.item_part_1_pos];
            const Shape& convex_part_2 = item_rotated_decompositions[e.item_2_pos][e.item_shape_2_pos][e.item_part_2_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_1,
                    {solution_bin.items[e.item_1_pos].bl_corner.x * sv, solution_bin.items[e.item_1_pos].bl_corner.y * sv},
                    1.0,  // scale
                    convex_part_2,
                    {solution_bin.items[e.item_2_pos].bl_corner.x * sv, solution_bin.items[e.item_2_pos].bl_corner.y * sv},
                    1.0);  // scale
            if (shape::equal(p.coef_x2, 0.0) && shape::equal(p.coef_y2, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "sep_i" + std::to_string(e.item_1_pos)
                    + "_s" + std::to_string(e.item_shape_1_pos)
                    + "_p" + std::to_string(e.item_part_1_pos)
                    + "_i" + std::to_string(e.item_2_pos)
                    + "_s" + std::to_string(e.item_shape_2_pos)
                    + "_p" + std::to_string(e.item_part_2_pos));
            lp_model.constraints_lower_bounds.push_back(-(p.coef_lambda1 + p.coef_lambda2));
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x1, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x1);
            }
            if (!shape::equal(p.coef_y1, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y1);
            }
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
        }

        //lp_model.feasiblity_tolerance = 1e-3;
        //lp_model.check(1);
        //lp_model.format(std::cout, 4);

        std::vector<double> lp_initial_solution(lp_model.variables_lower_bounds.size());
        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            lp_initial_solution[x[item_pos]] = solution_item.bl_corner.x * instance.parameters().scale_value;
            lp_initial_solution[y[item_pos]] = solution_item.bl_corner.y * instance.parameters().scale_value;
        }
        //lp_model.format_solution(std::cout, lp_initial_solution, 4);
        //lp_model.check_solution(lp_initial_solution, 1);

        ///////////
        // Solve //
        ///////////

        std::vector<double> lp_solution;
        if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
            Highs highs;
            mathoptsolverscmake::reduce_printout(highs);
            mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
            highs.setOptionValue("parallel", "off");
            //mathoptsolverscmake::set_log_file(highs, "highs.log");
            mathoptsolverscmake::load(highs, lp_model);
            mathoptsolverscmake::set_solution(highs, lp_initial_solution);
            //std::cout << "LP solve start" << std::endl;
            mathoptsolverscmake::solve(highs);
            //std::cout << "LP solve end" << std::endl;
            lp_solution = mathoptsolverscmake::get_solution(highs);
#else
            throw std::invalid_argument(FUNC_SIGNATURE);
#endif

        } else {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }

        double new_value = lp_model.evaluate_objective(lp_solution);

        // If the solution of the MILP model is not better, stop.
        if (!shape::strictly_greater(current_value, new_value))
            break;
        current_value = new_value;

        // Retrieve solution.
        Solution new_solution(instance);
        new_solution.add_bin(solution_bin.bin_type_id, 1);
        for (ItemPos pos = 0; pos < (ItemPos)solution_bin.items.size(); ++pos) {
            const SolutionItem& solution_item = solution_bin.items[pos];
            new_solution.add_item(
                    0,
                    solution_item.item_type_id,
                    {lp_solution[x[pos]] / instance.parameters().scale_value, lp_solution[y[pos]] / instance.parameters().scale_value},
                    solution_item.angle,
                    solution_item.mirror);
        }
        solution = new_solution;
    }

    return solution;
}

}

LinearProgrammingAnchorOutput packingsolver::irregular::linear_programming_anchor(
        const Solution& initial_solution,
        double x_weight,
        double y_weight,
        const LinearProgrammingAnchorParameters& parameters)
{
    //std::cout << "linear_programming" << std::endl;
    const Instance& instance = initial_solution.instance();
    LinearProgrammingAnchorOutput output(instance);

    const InstanceConvexDecomposition icd = compute_instance_convex_decomposition(instance);

    Solution solution(instance);
    for (BinPos bin_pos = 0;
            bin_pos < initial_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = initial_solution.bin(bin_pos);
        Solution new_solution = ::linear_programming_anchor(
                instance,
                initial_solution,
                x_weight,
                y_weight,
                parameters,
                bin_pos,
                icd);
        solution.append(
                new_solution,
                0,
                solution_bin.copies);
    }
    //initial_solution.format(std::cout, 1);
    //solution.format(std::cout, 1);
    output.solution = solution;

    //instance.write("lp_test.json");
    //initial_solution.write("lp_test_initial.json");
    //solution.write("lp_test_expected.json");
    //exit(1);
    //std::cout << "linear_programming end" << std::endl;
    return output;
}

LinearProgrammingMinimizeShrinkageOutput packingsolver::irregular::linear_programming_minimize_shrinkage(
        const Solution& initial_solution,
        const std::vector<double>& initial_lambda,
        const std::vector<Profit>& item_penalties,
        const LinearProgrammingMinimizeShrinkageParameters& parameters)
{
    const Instance& instance = initial_solution.instance();
    LinearProgrammingMinimizeShrinkageOutput output(instance);

    const InstanceConvexDecomposition icd = compute_instance_convex_decomposition(instance);

    const BinTypeId bin_type_id = initial_solution.bin(0).bin_type_id;
    const BinType& bin_type = instance.bin_type(bin_type_id);

    std::vector<AxisAlignedBoundingBox> border_part_aabbs;
    for (const Shape& part: icd.bin_types_borders[bin_type_id])
        border_part_aabbs.push_back(part.compute_min_max());
    std::vector<AxisAlignedBoundingBox> defect_part_aabbs;
    for (const Shape& part: icd.bin_types_defects[bin_type_id])
        defect_part_aabbs.push_back(part.compute_min_max());

    const LengthDbl sv = instance.parameters().scale_value;
    const LengthDbl movement_box_half_size = 10;
    const double lambda_delta = 0.2;

    Solution solution(instance);
    solution.append(initial_solution, 0, 1);

    double current_value = 0;
    std::vector<bool> current_items_shrunken(solution.bin(0).items.size(), true);
    std::vector<double> current_lambda = initial_lambda;

    for (Counter number_of_iterations = 0;; ++number_of_iterations) {
        //std::cout << "number_of_iterations " << number_of_iterations << std::endl;
        const SolutionBin& solution_bin = solution.bin(0);

        // Precompute rotated convex decompositions.
        std::vector<std::vector<std::vector<Shape>>> item_rotated_decompositions(
                solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(solution_item.item_type_id);
            item_rotated_decompositions[item_pos].resize(item_type.shapes.size());
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const Shape& convex_part: icd.item_types[solution_item.item_type_id][item_shape_pos]) {
                    Shape rotated = convex_part;
                    if (solution_item.mirror)
                        rotated = rotated.axial_symmetry_y_axis();
                    rotated = rotated.rotate(solution_item.angle);
                    item_rotated_decompositions[item_pos][item_shape_pos].push_back(rotated);
                }
            }
        }

        ////////////////////
        // Setup LP model //
        ////////////////////

        mathoptsolverscmake::MilpModel lp_model;

        /////////////////////////////
        // Objective and variables //
        /////////////////////////////

        // Position variables (objective coefficient 0) and lambda variables
        // (objective coefficient -penalty; minimizing -sum(lambda*penalty) maximizes lambda).
        lp_model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
        std::vector<int> x(solution_bin.items.size());
        std::vector<int> y(solution_bin.items.size());
        std::vector<int> lambda_var(solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];

            x[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(-std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_upper_bounds.push_back(+std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(0.0);
            lp_model.variables_names.push_back("x_" + std::to_string(item_pos));

            y[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(-std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_upper_bounds.push_back(+std::numeric_limits<LengthDbl>::infinity());
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(0.0);
            lp_model.variables_names.push_back("y_" + std::to_string(item_pos));

            lambda_var[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(0.0);
            lp_model.variables_upper_bounds.push_back(1.0);
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            lp_model.objective_coefficients.push_back(item_penalties[item_pos]);
            lp_model.variables_names.push_back("lambda_" + std::to_string(item_pos));
        }

        /////////////////
        // Constraints //
        /////////////////

        std::vector<ItemAxisAlignedBoundingBoxes> item_aabbs = compute_item_aabbs(
                instance,
                solution_bin,
                movement_box_half_size,
                item_rotated_decompositions,
                current_lambda);

        // Bin containment per convex part: x_j + x_max_l_p * lambda_j <= x_max_p, etc.
        // Coefficients come from part_aabbs; RHS from part_movement_aabbs clamped to bin.
        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
            for (ItemShapePos shape_pos = 0;
                    shape_pos < (ItemShapePos)item_aabbs[item_pos].part_aabbs.size();
                    ++shape_pos) {
                for (Counter part_pos = 0;
                        part_pos < (Counter)item_aabbs[item_pos].part_aabbs[shape_pos].size();
                        ++part_pos) {
                    const AxisAlignedBoundingBox& pa =
                            item_aabbs[item_pos].part_aabbs[shape_pos][part_pos];
                    const AxisAlignedBoundingBox& pm =
                            item_aabbs[item_pos].part_movement_aabbs[shape_pos][part_pos];

                    // RHS: part movement AABB clamped to bin bounds.
                    const LengthDbl x_min = (std::max)(pm.x_min, bin_type.aabb.x_min * sv);
                    const LengthDbl x_max = (std::min)(pm.x_max, bin_type.aabb.x_max * sv);
                    const LengthDbl y_min = (std::max)(pm.y_min, bin_type.aabb.y_min * sv);
                    const LengthDbl y_max = (std::min)(pm.y_max, bin_type.aabb.y_max * sv);

                    const std::string sid = std::to_string(item_pos)
                            + "_s" + std::to_string(shape_pos)
                            + "_p" + std::to_string(part_pos);

                    // x_j + x_max_l_p * lambda_j <= x_max_p
                    lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
                    lp_model.constraints_names.push_back("bin_x_max_i" + sid);
                    lp_model.constraints_lower_bounds.push_back(-std::numeric_limits<LengthDbl>::infinity());
                    lp_model.constraints_upper_bounds.push_back(x_max);
                    lp_model.elements_variables.push_back(x[item_pos]);
                    lp_model.elements_coefficients.push_back(1.0);
                    lp_model.elements_variables.push_back(lambda_var[item_pos]);
                    lp_model.elements_coefficients.push_back(pa.x_max);

                    // x_j + x_min_l_p * lambda_j >= x_min_p
                    lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
                    lp_model.constraints_names.push_back("bin_x_min_i" + sid);
                    lp_model.constraints_lower_bounds.push_back(x_min);
                    lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
                    lp_model.elements_variables.push_back(x[item_pos]);
                    lp_model.elements_coefficients.push_back(1.0);
                    lp_model.elements_variables.push_back(lambda_var[item_pos]);
                    lp_model.elements_coefficients.push_back(pa.x_min);

                    // y_j + y_max_l_p * lambda_j <= y_max_p
                    lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
                    lp_model.constraints_names.push_back("bin_y_max_i" + sid);
                    lp_model.constraints_lower_bounds.push_back(-std::numeric_limits<LengthDbl>::infinity());
                    lp_model.constraints_upper_bounds.push_back(y_max);
                    lp_model.elements_variables.push_back(y[item_pos]);
                    lp_model.elements_coefficients.push_back(1.0);
                    lp_model.elements_variables.push_back(lambda_var[item_pos]);
                    lp_model.elements_coefficients.push_back(pa.y_max);

                    // y_j + y_min_l_p * lambda_j >= y_min_p
                    lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
                    lp_model.constraints_names.push_back("bin_y_min_i" + sid);
                    lp_model.constraints_lower_bounds.push_back(y_min);
                    lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
                    lp_model.elements_variables.push_back(y[item_pos]);
                    lp_model.elements_coefficients.push_back(1.0);
                    lp_model.elements_variables.push_back(lambda_var[item_pos]);
                    lp_model.elements_coefficients.push_back(pa.y_min);
                }
            }
        }

        // Obstacle (shape_1) is at world origin with lambda=1 (fixed).
        // Item (shape_2) has LP variables x, y, lambda.
        // Full constraint: coef_x1*0 + coef_y1*0 + coef_lambda1*1
        //                + coef_x2*x + coef_y2*y + coef_lambda2*lambda >= 0
        //  => coef_x2*x + coef_y2*y + coef_lambda2*lambda >= -coef_lambda1

        IntersectingParts intersecting_parts = compute_potentially_intersecting_parts(
                item_aabbs,
                border_part_aabbs,
                defect_part_aabbs);

        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_border_part) {
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    icd.bin_types_borders[bin_type_id][e.defect_pos],
                    {0, 0},
                    1.0,
                    item_rotated_decompositions[e.item_pos][e.item_shape_pos][e.item_part_pos],
                    {solution_bin.items[e.item_pos].bl_corner.x * sv, solution_bin.items[e.item_pos].bl_corner.y * sv},
                    current_lambda[e.item_pos]);
            if (shape::equal(p.coef_x2, 0.0) && shape::equal(p.coef_y2, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "border_i" + std::to_string(e.item_pos)
                    + "_s" + std::to_string(e.item_shape_pos)
                    + "_p" + std::to_string(e.item_part_pos)
                    + "_bp" + std::to_string(e.defect_pos));
            lp_model.constraints_lower_bounds.push_back(-p.coef_lambda1);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
            if (!shape::equal(p.coef_lambda2, 0.0)) {
                lp_model.elements_variables.push_back(lambda_var[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_lambda2);
            }
        }

        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_defect_part) {
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    icd.bin_types_defects[bin_type_id][e.defect_pos],
                    {0, 0},
                    1.0,
                    item_rotated_decompositions[e.item_pos][e.item_shape_pos][e.item_part_pos],
                    {solution_bin.items[e.item_pos].bl_corner.x * sv, solution_bin.items[e.item_pos].bl_corner.y * sv},
                    current_lambda[e.item_pos]);
            if (shape::equal(p.coef_x2, 0.0) && shape::equal(p.coef_y2, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(
                    "defect_i" + std::to_string(e.item_pos)
                    + "_s" + std::to_string(e.item_shape_pos)
                    + "_p" + std::to_string(e.item_part_pos)
                    + "_dp" + std::to_string(e.defect_pos));
            lp_model.constraints_lower_bounds.push_back(-p.coef_lambda1);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
            if (!shape::equal(p.coef_lambda2, 0.0)) {
                lp_model.elements_variables.push_back(lambda_var[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.coef_lambda2);
            }
        }

        for (const IntersectingItemParts& e: intersecting_parts.intersecting_item_parts) {
            const Shape& convex_part_1 = item_rotated_decompositions[e.item_1_pos][e.item_shape_1_pos][e.item_part_1_pos];
            const Shape& convex_part_2 = item_rotated_decompositions[e.item_2_pos][e.item_shape_2_pos][e.item_part_2_pos];
            const SolutionItem& item_1 = solution_bin.items[e.item_1_pos];
            const SolutionItem& item_2 = solution_bin.items[e.item_2_pos];
            std::string constraint_name
                = "sep_i" + std::to_string(e.item_1_pos)
                + "_s" + std::to_string(e.item_shape_1_pos)
                + "_p" + std::to_string(e.item_part_1_pos)
                + "_i" + std::to_string(e.item_2_pos)
                + "_s" + std::to_string(e.item_shape_2_pos)
                + "_p" + std::to_string(e.item_part_2_pos);
            //std::cout << "item_1_pos " << e.item_1_pos
            //    << " item_type_1 " << item_1.item_type_id
            //    << " item_1_shape_pos " << e.item_shape_1_pos
            //    << " item_1_part_pos " << e.item_part_1_pos
            //    << " bl_corner " << (sv * item_1.bl_corner).to_string()
            //    << " angle " << item_1.angle
            //    << " mirror " << item_1.mirror
            //    << std::endl;
            //std::cout << "item_2_pos " << e.item_2_pos
            //    << " item_type_2 " << item_2.item_type_id
            //    << " item_2_shape_pos " << e.item_shape_2_pos
            //    << " item_2_part_pos " << e.item_part_2_pos
            //    << " bl_corner " << (sv * item_2.bl_corner).to_string()
            //    << " angle " << item_2.angle
            //    << " mirror " << item_2.mirror
            //    << std::endl;
            //std::cout << "constraint " << constraint_name << std::endl;
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_1,
                    {item_1.bl_corner.x * sv, item_1.bl_corner.y * sv},
                    current_lambda[e.item_1_pos],
                    convex_part_2,
                    {item_2.bl_corner.x * sv, item_2.bl_corner.y * sv},
                    current_lambda[e.item_2_pos]);
            //std::cout << "shape_pos " << p.edge_shape_pos
            //    << " edge_element_pos " << p.edge_element_pos
            //    << " point_element_pos " << p.point_element_pos
            //    << " distance " << p.distance
            //    << std::endl;
            //std::cout << "edge_element " << ((p.edge_shape_pos == 0)?
            //        convex_part_1.elements[p.edge_element_pos].to_string():
            //        convex_part_2.elements[p.edge_element_pos].to_string()) << std::endl;
            //std::cout << "point_element " << ((p.edge_shape_pos == 0)?
            //        convex_part_2.elements[p.point_element_pos].to_string():
            //        convex_part_1.elements[p.point_element_pos].to_string()) << std::endl;
            //std::cout << "point " << p.point.to_string() << std::endl;
            //std::cout << "coef_x1 " << p.coef_x1 << " coef_y1 " << p.coef_y1 << " coef_lambda1 " << p.coef_lambda1 << std::endl;
            //std::cout << "coef_x2 " << p.coef_x2 << " coef_y2 " << p.coef_y2 << " coef_lambda2 " << p.coef_lambda2 << std::endl;
            if (shape::equal(p.coef_x2, 0.0) && shape::equal(p.coef_y2, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_names.push_back(constraint_name);
            lp_model.constraints_lower_bounds.push_back(0.0);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.coef_x1, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x1);
            }
            if (!shape::equal(p.coef_y1, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y1);
            }
            if (!shape::equal(p.coef_lambda1, 0.0)) {
                lp_model.elements_variables.push_back(lambda_var[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(p.coef_lambda1);
            }
            if (!shape::equal(p.coef_x2, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_x2);
            }
            if (!shape::equal(p.coef_y2, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_y2);
            }
            if (!shape::equal(p.coef_lambda2, 0.0)) {
                lp_model.elements_variables.push_back(lambda_var[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.coef_lambda2);
            }
        }

        std::vector<double> lp_initial_solution(lp_model.variables_lower_bounds.size(), 0.0);
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            lp_initial_solution[x[item_pos]] = solution_item.bl_corner.x * sv;
            lp_initial_solution[y[item_pos]] = solution_item.bl_corner.y * sv;
            lp_initial_solution[lambda_var[item_pos]] = current_lambda[item_pos];
        }

        lp_model.feasibility_tolerance = 1e-6;
        lp_model.integrality_tolerance = 1e-10;

        //lp_model.format(std::cout, 4);
        //std::cout << lp_model.check_solution(lp_initial_solution, 4) << std::endl;
        //std::cout << lp_model.check(4) << std::endl;

        ///////////
        // Solve //
        ///////////

        //std::cout << "solve" << std::endl;

        std::vector<double> lp_solution;
        if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
            Highs highs;
            mathoptsolverscmake::reduce_printout(highs);
            mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
            highs.setOptionValue("parallel", "off");
            mathoptsolverscmake::load(highs, lp_model);
            //mathoptsolverscmake::write_mps(highs, "lp.mps");
            mathoptsolverscmake::set_solution(highs, lp_initial_solution);
            lp_model.write_solution(lp_initial_solution, "initial_solution.txt");
            mathoptsolverscmake::solve(highs);
            if (highs.getModelStatus() == HighsModelStatus::kInfeasible
                    || highs.getModelStatus() == HighsModelStatus::kUnboundedOrInfeasible) {
                highs.writeModel("infeasible.mps");
                std::cerr << "linear_programming_minimize_shrinkage: LP infeasible, wrote infeasible.mps" << std::endl;
                exit(1);
            }
            lp_solution = mathoptsolverscmake::get_solution(highs);
#else
            throw std::invalid_argument(FUNC_SIGNATURE);
#endif
        } else {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }

        if (!lp_model.check_solution(lp_solution, 0)) {
            lp_model.check_solution(lp_solution, 4);
            throw std::logic_error(
                    FUNC_SIGNATURE + ": wrong LP solution.");
        }
        //std::cout << "check " << lp_model.check_solution(lp_solution, 4) << std::endl;;
        double new_value = lp_model.evaluate_objective(lp_solution);

        // Compute items_shrunken and lambda values for this LP solution.
        std::vector<bool> new_items_shrunken(solution_bin.items.size(), false);
        std::vector<double> new_lambda(solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            new_lambda[item_pos] = lp_solution[lambda_var[item_pos]];
            new_items_shrunken[item_pos] = new_lambda[item_pos] < 1.0 - 1e-6;
            //std::cout << "item_pos " << item_pos
            //    << " lambda " << new_lambda[item_pos]
            //    << " shrunken " << new_items_shrunken[item_pos]
            //    << std::endl;
        }

        if (!shape::strictly_greater(new_value, current_value))
            break;

        // Retrieve solution.
        Solution new_solution(instance);
        new_solution.add_bin(solution_bin.bin_type_id, 1);
        for (ItemPos pos = 0; pos < (ItemPos)solution_bin.items.size(); ++pos) {
            const SolutionItem& solution_item = solution_bin.items[pos];
            new_solution.add_item(
                    0,
                    solution_item.item_type_id,
                    {lp_solution[x[pos]] / sv, lp_solution[y[pos]] / sv},
                    solution_item.angle,
                    solution_item.mirror);
        }

        // Check intersections between convex parts.
        for (const IntersectingItemParts& e: intersecting_parts.intersecting_item_parts) {
            const SolutionBin& bin_prev = solution.bin(0);
            const SolutionBin& bin_curr = new_solution.bin(0);
            const Shape& convex_part_1 = item_rotated_decompositions[e.item_1_pos][e.item_shape_1_pos][e.item_part_1_pos];
            const Shape& convex_part_2 = item_rotated_decompositions[e.item_2_pos][e.item_shape_2_pos][e.item_part_2_pos];
            Shape convex_part_1_prev = convex_part_1;
            Shape convex_part_2_prev = convex_part_2;
            Shape convex_part_1_curr = convex_part_1;
            Shape convex_part_2_curr = convex_part_2;
            convex_part_1_prev = current_lambda[e.item_1_pos] * convex_part_1_prev;
            convex_part_1_curr = new_lambda[e.item_1_pos] * convex_part_1_curr;
            convex_part_2_prev = current_lambda[e.item_2_pos] * convex_part_2_prev;
            convex_part_2_curr = new_lambda[e.item_2_pos] * convex_part_2_curr;
            const SolutionItem& item_1_prev = bin_prev.items[e.item_1_pos];
            const SolutionItem& item_2_prev = bin_prev.items[e.item_2_pos];
            const SolutionItem& item_1_curr = bin_curr.items[e.item_1_pos];
            const SolutionItem& item_2_curr = bin_curr.items[e.item_2_pos];
            convex_part_1_prev.shift(item_1_prev.bl_corner.x * sv, item_1_prev.bl_corner.y * sv);
            convex_part_1_curr.shift(item_1_curr.bl_corner.x * sv, item_1_curr.bl_corner.y * sv);
            convex_part_2_prev.shift(item_2_prev.bl_corner.x * sv, item_2_prev.bl_corner.y * sv);
            convex_part_2_curr.shift(item_2_curr.bl_corner.x * sv, item_2_curr.bl_corner.y * sv);
            if (equal(new_lambda[e.item_1_pos], 0.0)) {
                if (equal(new_lambda[e.item_2_pos], 0.0)) {
                    if (!equal(item_1_curr.bl_corner, item_2_curr.bl_corner))
                        continue;
                } else {
                    if (!convex_part_2_curr.contains(item_1_curr.bl_corner, true))
                        continue;
                }
            } else {
                if (equal(new_lambda[e.item_2_pos], 0.0)) {
                    if (!convex_part_1_curr.contains(item_2_curr.bl_corner, true))
                        continue;
                } else {
                    if (!intersect(convex_part_1_curr, convex_part_2_curr, true))
                        continue;
                }
            }

            std::string constraint_name
                = "sep_i" + std::to_string(e.item_1_pos)
                + "_s" + std::to_string(e.item_shape_1_pos)
                + "_p" + std::to_string(e.item_part_1_pos)
                + "_i" + std::to_string(e.item_2_pos)
                + "_s" + std::to_string(e.item_shape_2_pos)
                + "_p" + std::to_string(e.item_part_2_pos);
            std::cout << "constraint " << constraint_name << std::endl;

            std::cout << "item_1_pos " << e.item_1_pos
                << " item_type_1 " << item_1_prev.item_type_id
                << " item_1_shape_pos " << e.item_shape_1_pos
                << " item_1_part_pos " << e.item_part_1_pos
                << " angle " << item_1_prev.angle
                << " mirror " << item_1_prev.mirror
                << std::endl;
            std::cout << " bl " << (sv * item_1_prev.bl_corner).to_string()
                << " -> " << (sv * item_1_curr.bl_corner).to_string() << std::endl;
            std::cout << " lambda " << current_lambda[e.item_1_pos]
                << " -> " << new_lambda[e.item_1_pos] << std::endl;
            std::cout << "item_2_pos " << e.item_2_pos
                << " item_type_2 " << item_2_prev.item_type_id
                << " item_2_shape_pos " << e.item_shape_2_pos
                << " item_2_part_pos " << e.item_part_2_pos
                << " angle " << item_2_prev.angle
                << " mirror " << item_2_prev.mirror
                << std::endl;
            std::cout << " bl " << (sv * item_2_prev.bl_corner).to_string()
                << " -> " << (sv * item_2_curr.bl_corner).to_string() << std::endl;
            std::cout << " lambda " << current_lambda[e.item_2_pos]
                << " -> " << new_lambda[e.item_2_pos] << std::endl;

            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_1,
                    {item_1_prev.bl_corner.x * sv, item_1_prev.bl_corner.y * sv},
                    current_lambda[e.item_1_pos],
                    convex_part_2,
                    {item_2_prev.bl_corner.x * sv, item_2_prev.bl_corner.y * sv},
                    current_lambda[e.item_2_pos]);
            std::cout << "shape_pos " << p.edge_shape_pos
                << " edge_element_pos " << p.edge_element_pos
                << " point_element_pos " << p.point_element_pos
                << " distance " << p.distance
                << std::endl;
            std::cout << "edge_element " << ((p.edge_shape_pos == 0)?
                    convex_part_1.elements[p.edge_element_pos].to_string():
                    convex_part_2.elements[p.edge_element_pos].to_string()) << std::endl;
            std::cout << "point_element " << ((p.edge_shape_pos == 0)?
                    convex_part_2.elements[p.point_element_pos].to_string():
                    convex_part_1.elements[p.point_element_pos].to_string()) << std::endl;
            std::cout << "point " << p.point.to_string() << std::endl;
            std::cout << "coef_x1 " << p.coef_x1 << " coef_y1 " << p.coef_y1 << " coef_lambda1 " << p.coef_lambda1 << std::endl;
            std::cout << "coef_x2 " << p.coef_x2 << " coef_y2 " << p.coef_y2 << " coef_lambda2 " << p.coef_lambda2 << std::endl;

            Writer()
                .add_shape(convex_part_1, "Part 1")
                .add_shape(convex_part_2, "Part 2")
                .add_shape(convex_part_1_prev, "Part 1 (prev)")
                .add_shape(convex_part_1_curr, "Part 1 (curr)")
                .add_shape(convex_part_2_prev, "Part 2 (prev)")
                .add_shape(convex_part_2_curr, "Part 2 (curr)")
                .write_json("tmp.json");
            throw std::logic_error(
                    FUNC_SIGNATURE + ": convex part intersection after LP.");
        }

        Solution::OverlappingItems overlapping_items = new_solution.compute_overlapping_items(0, &new_lambda);
        if (!overlapping_items.item_item_pairs.empty()) {
            const SolutionBin& bin = new_solution.bin(0);
            for (const auto& pair: overlapping_items.item_item_pairs) {
                const SolutionItem& item_1 = bin.items[pair.first];
                const ItemType& item_type_1 = instance.item_type(item_1.item_type_id);
                ShapeWithHoles shape_1 = new_lambda[pair.first] * new_solution.shape_scaled(0, pair.first, 0);
                ShapeWithHoles shape_1_prev = current_lambda[pair.first] * solution.shape_scaled(0, pair.first, 0);

                const SolutionItem& item_2 = bin.items[pair.second];
                const ItemType& item_type_2 = instance.item_type(item_2.item_type_id);
                ShapeWithHoles shape_2 = new_lambda[pair.second] * new_solution.shape_scaled(0, pair.second, 0);
                ShapeWithHoles shape_2_prev = current_lambda[pair.second] * solution.shape_scaled(0, pair.second, 0);

                Writer()
                    .add_shape_with_holes(shape_1_prev, "Shape 1 (prev)")
                    .add_shape_with_holes(shape_1, "Shape 1 (curr)")
                    .add_shape_with_holes(shape_2_prev, "Shape 2 (prev)")
                    .add_shape_with_holes(shape_2, "Shape 2 (curr)")
                    .write_json("tmp.json");
            }
            throw std::logic_error(
                    FUNC_SIGNATURE + ": infeasible new_solution after LP.");
        }

        solution = new_solution;
        current_value = new_value;
        current_items_shrunken = new_items_shrunken;
        current_lambda = new_lambda;
    }

    output.solution = solution;
    output.items_shrunken = current_items_shrunken;
    output.final_lambda = current_lambda;
    output.feasible = std::none_of(
            current_items_shrunken.begin(),
            current_items_shrunken.end(),
            [](bool s) { return s; });

    return output;
}
