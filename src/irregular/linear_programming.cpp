#include "irregular/linear_programming.hpp"

#include "shape/convex_partition.hpp"
#include "shape/intersection_tree.hpp"

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

struct EdgeSeparationConstraintParameters
{
    LengthDbl a;
    LengthDbl b;
    LengthDbl rhs;
};

/**
 * Given the best separating edge (element_pos from shape_from, separating
 * it from shape_to), compute the LP constraint parameters a, b, rhs.
 *
 * a and b are the components of the edge outward normal.
 * rhs encodes the required signed distance: a*cx_from + b*cy_from >= rhs
 * (in relative coordinates where shape_to is at the origin).
 */
EdgeSeparationConstraintParameters compute_edge_separation_constraint_parameters(
        const Shape& shape_from,
        const Shape& shape_to,
        ElementPos element_pos)
{
    EdgeSeparationConstraintParameters output;
    const ShapeElement& element = shape_from.elements[element_pos];
    output.a = element.end.y - element.start.y;
    output.b = element.start.x - element.end.x;
    LengthDbl min_proj = std::numeric_limits<LengthDbl>::infinity();
    for (const ShapeElement& element_to: shape_to.elements)
        min_proj = std::min(min_proj, output.a * element_to.start.x + output.b * element_to.start.y);
    output.rhs = output.a * element.start.x + output.b * element.start.y - min_proj;
    return output;
}

/**
 * Given two convex shapes at world positions (cx_1, cy_1) and (cx_2, cy_2),
 * find the edge (from either shape) that gives the maximum minimum signed
 * distance as a separating hyperplane in the current configuration.
 *
 * Returns EdgeSeparationConstraintParameters normalized so the non-overlap
 * constraint is always:
 *   a*(cx_2 - cx_1) + b*(cy_2 - cy_1) >= rhs
 * When the best edge comes from shape_2, a and b are negated to preserve
 * the inequality direction.
 */
EdgeSeparationConstraintParameters find_best_edge_separator(
        const Shape& shape_1,
        const Shape& shape_2,
        LengthDbl cx_1,
        LengthDbl cy_1,
        LengthDbl cx_2,
        LengthDbl cy_2)
{
    const LengthDbl dx = cx_2 - cx_1;
    const LengthDbl dy = cy_2 - cy_1;
    LengthDbl best_min_dist = -std::numeric_limits<LengthDbl>::infinity();
    EdgeSeparationConstraintParameters p_best;

    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_1.elements.size();
            ++element_pos) {
        EdgeSeparationConstraintParameters p = compute_edge_separation_constraint_parameters(shape_1, shape_2, element_pos);
        LengthDbl min_dist = p.a * dx + p.b * dy - p.rhs;
        if (min_dist > best_min_dist) {
            best_min_dist = min_dist;
            p_best = p;
        }
    }

    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_2.elements.size();
            ++element_pos) {
        EdgeSeparationConstraintParameters p = compute_edge_separation_constraint_parameters(shape_2, shape_1, element_pos);
        p.a = -p.a;
        p.b = -p.b;
        LengthDbl min_dist = p.a * dx + p.b * dy - p.rhs;
        if (min_dist > best_min_dist) {
            best_min_dist = min_dist;
            p_best = p;
        }
    }

    return p_best;
}

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
 * Compute world AABBs for each item convex part, shifted to the item's current
 * position and expanded by box_half (in scaled coordinates) on all sides.
 */
std::vector<std::vector<std::vector<AxisAlignedBoundingBox>>> compute_item_world_aabbs(
        const Instance& instance,
        const SolutionBin& solution_bin,
        LengthDbl movement_box_half_size,
        const std::vector<std::vector<std::vector<AxisAlignedBoundingBox>>>& item_part_aabbs)
{
    std::vector<std::vector<std::vector<AxisAlignedBoundingBox>>> item_world_aabbs(
            solution_bin.items.size());
    for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
        const LengthDbl cx = solution_bin.items[item_pos].bl_corner.x * instance.parameters().scale_value;
        const LengthDbl cy = solution_bin.items[item_pos].bl_corner.y * instance.parameters().scale_value;
        item_world_aabbs[item_pos].resize(item_part_aabbs[item_pos].size());
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_part_aabbs[item_pos].size();
                ++item_shape_pos) {
            item_world_aabbs[item_pos][item_shape_pos].resize(
                    item_part_aabbs[item_pos][item_shape_pos].size());
            for (Counter part_pos = 0;
                    part_pos < (Counter)item_part_aabbs[item_pos][item_shape_pos].size();
                    ++part_pos) {
                AxisAlignedBoundingBox aabb = item_part_aabbs[item_pos][item_shape_pos][part_pos];
                aabb.shift({cx, cy});
                aabb.x_min -= movement_box_half_size;
                aabb.x_max += movement_box_half_size;
                aabb.y_min -= movement_box_half_size;
                aabb.y_max += movement_box_half_size;
                item_world_aabbs[item_pos][item_shape_pos][part_pos] = aabb;
            }
        }
    }
    return item_world_aabbs;
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
        const std::vector<std::vector<std::vector<AxisAlignedBoundingBox>>>& item_world_aabbs,
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
    for (ItemPos item_pos = 0; item_pos < (ItemPos)item_world_aabbs.size(); ++item_pos) {
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_world_aabbs[item_pos].size();
                ++item_shape_pos) {
            for (Counter part_pos = 0;
                    part_pos < (Counter)item_world_aabbs[item_pos][item_shape_pos].size();
                    ++part_pos) {
                shape_index_map.push_back({ShapeType::Item, item_pos, item_shape_pos, part_pos});
                shape::ShapeWithHoles swh;
                swh.shape = shape::build_rectangle(
                        item_world_aabbs[item_pos][item_shape_pos][part_pos]);
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

Solution linear_programming(
        const Instance& instance,
        const Solution& initial_solution,
        const LinearProgrammingAnchorToCornerParameters& parameters,
        BinPos bin_pos,
        const InstanceConvexDecomposition& icd)
{
    Solution solution(instance);
    solution.append(initial_solution, bin_pos, 1);
    double current_value = 0;
    for (const SolutionItem& solution_item: initial_solution.bin(bin_pos).items) {
        if (parameters.anchor_corner == Corner::BottomLeft
                || parameters.anchor_corner == Corner::TopLeft) {
            current_value += solution_item.bl_corner.x * instance.parameters().scale_value;
        } else {
            current_value -= solution_item.bl_corner.x * instance.parameters().scale_value;
        }
        if (parameters.anchor_corner == Corner::BottomLeft
                || parameters.anchor_corner == Corner::BottomRight) {
            current_value += solution_item.bl_corner.y * instance.parameters().scale_value;
        } else {
            current_value -= solution_item.bl_corner.y * instance.parameters().scale_value;
        }
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

        // Precompute AABBs for rotated item convex parts (local coordinates,
        // before adding bl_corner).
        std::vector<std::vector<std::vector<AxisAlignedBoundingBox>>> item_part_aabbs(solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const ItemType& item_type = instance.item_type(
                    solution_bin.items[item_pos].item_type_id);
            item_part_aabbs[item_pos].resize(item_type.shapes.size());
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const Shape& part: item_rotated_decompositions[item_pos][item_shape_pos])
                    item_part_aabbs[item_pos][item_shape_pos].push_back(
                            part.compute_min_max());
            }
        }

        // Precompute per-item bin bounds (without movement restriction) and
        // current scaled positions. The local bounding box of each item is
        // read from the already-computed item_part_aabbs.
        std::vector<AxisAlignedBoundingBox> item_bin_bounds(solution_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)solution_bin.items.size();
                ++item_pos) {
            const SolutionItem& solution_item = solution_bin.items[item_pos];
            const ItemType& item_type = instance.item_type(solution_item.item_type_id);

            LengthDbl x_min_local = std::numeric_limits<LengthDbl>::infinity();
            LengthDbl x_max_local = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_min_local = std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_max_local = -std::numeric_limits<LengthDbl>::infinity();
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                for (const AxisAlignedBoundingBox& aabb: item_part_aabbs[item_pos][item_shape_pos]) {
                    x_min_local = std::min(x_min_local, aabb.x_min);
                    x_max_local = std::max(x_max_local, aabb.x_max);
                    y_min_local = std::min(y_min_local, aabb.y_min);
                    y_max_local = std::max(y_max_local, aabb.y_max);
                }
            }

            item_bin_bounds[item_pos].x_min = bin_type.aabb.x_min * instance.parameters().scale_value - x_min_local;
            item_bin_bounds[item_pos].x_max = bin_type.aabb.x_max * instance.parameters().scale_value - x_max_local;
            item_bin_bounds[item_pos].y_min = bin_type.aabb.y_min * instance.parameters().scale_value - y_min_local;
            item_bin_bounds[item_pos].y_max = bin_type.aabb.y_max * instance.parameters().scale_value - y_max_local;
        }

        IntersectingParts intersecting_parts = compute_potentially_intersecting_parts(
                compute_item_world_aabbs(instance, solution_bin, movement_box_half_size, item_part_aabbs),
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
            if (parameters.anchor_corner == Corner::BottomLeft
                    || parameters.anchor_corner == Corner::TopLeft) {
                lp_model.objective_coefficients.push_back(1);
            } else {
                lp_model.objective_coefficients.push_back(-1);
            }
            y[item_pos] = lp_model.variables_lower_bounds.size();
            lp_model.variables_lower_bounds.push_back(std::max(
                    item_bin_bounds[item_pos].y_min,
                    solution_item.bl_corner.y * instance.parameters().scale_value - movement_box_half_size));
            lp_model.variables_upper_bounds.push_back(std::min(
                    item_bin_bounds[item_pos].y_max,
                    solution_item.bl_corner.y * instance.parameters().scale_value + movement_box_half_size));
            lp_model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
            if (parameters.anchor_corner == Corner::BottomLeft
                    || parameters.anchor_corner == Corner::BottomRight) {
                lp_model.objective_coefficients.push_back(1);
            } else {
                lp_model.objective_coefficients.push_back(-1);
            }
        }

        /////////////////
        // Constraints //
        /////////////////

        // Intersections between items and bin borders.
        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_border_part) {
            const Shape& convex_part_item = item_rotated_decompositions[e.item_pos][e.item_shape_pos][e.item_part_pos];
            const Shape& convex_part_obs = icd.bin_types_borders[bin_type_id][e.defect_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_obs,
                    convex_part_item,
                    0,
                    0,
                    solution_bin.items[e.item_pos].bl_corner.x * instance.parameters().scale_value,
                    solution_bin.items[e.item_pos].bl_corner.y * instance.parameters().scale_value);
            if (shape::equal(p.a, 0.0) && shape::equal(p.b, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_lower_bounds.push_back(p.rhs);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.a, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.a);
            }
            if (!shape::equal(p.b, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.b);
            }
        }

        // Intersections between items and bin defects.
        for (const IntersectingItemPartDefectPart& e: intersecting_parts.intersecting_item_part_defect_part) {
            const Shape& convex_part_item = item_rotated_decompositions[e.item_pos][e.item_shape_pos][e.item_part_pos];
            const Shape& convex_part_obs = icd.bin_types_defects[bin_type_id][e.defect_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_obs,
                    convex_part_item,
                    0,
                    0,
                    solution_bin.items[e.item_pos].bl_corner.x * instance.parameters().scale_value,
                    solution_bin.items[e.item_pos].bl_corner.y * instance.parameters().scale_value);
            if (shape::equal(p.a, 0.0) && shape::equal(p.b, 0.0))
                continue;
            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_lower_bounds.push_back(p.rhs);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.a, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.a);
            }
            if (!shape::equal(p.b, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_pos]);
                lp_model.elements_coefficients.push_back(p.b);
            }
        }

        // Intersections between pairs of items.
        for (const IntersectingItemParts& e: intersecting_parts.intersecting_item_parts) {
            const Shape& convex_part_1 = item_rotated_decompositions[e.item_1_pos][e.item_shape_1_pos][e.item_part_1_pos];
            const Shape& convex_part_2 = item_rotated_decompositions[e.item_2_pos][e.item_shape_2_pos][e.item_part_2_pos];
            const EdgeSeparationConstraintParameters p = find_best_edge_separator(
                    convex_part_1,
                    convex_part_2,
                    solution_bin.items[e.item_1_pos].bl_corner.x * instance.parameters().scale_value,
                    solution_bin.items[e.item_1_pos].bl_corner.y * instance.parameters().scale_value,
                    solution_bin.items[e.item_2_pos].bl_corner.x * instance.parameters().scale_value,
                    solution_bin.items[e.item_2_pos].bl_corner.y * instance.parameters().scale_value);

            if (shape::equal(p.a, 0.0) && shape::equal(p.b, 0.0))
                continue;

            lp_model.constraints_starts.push_back(lp_model.elements_variables.size());
            lp_model.constraints_lower_bounds.push_back(p.rhs);
            lp_model.constraints_upper_bounds.push_back(std::numeric_limits<LengthDbl>::infinity());
            if (!shape::equal(p.a, 0.0)) {
                lp_model.elements_variables.push_back(x[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.a);
                lp_model.elements_variables.push_back(x[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(-p.a);
            }
            if (!shape::equal(p.b, 0.0)) {
                lp_model.elements_variables.push_back(y[e.item_2_pos]);
                lp_model.elements_coefficients.push_back(p.b);
                lp_model.elements_variables.push_back(y[e.item_1_pos]);
                lp_model.elements_coefficients.push_back(-p.b);
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

LinearProgrammingAnchorToCornerOutput packingsolver::irregular::linear_programming_anchor_to_corner(
        const Solution& initial_solution,
        const LinearProgrammingAnchorToCornerParameters& parameters)
{
    //std::cout << "linear_programming" << std::endl;
    const Instance& instance = initial_solution.instance();
    LinearProgrammingAnchorToCornerOutput output(instance);

    const InstanceConvexDecomposition icd = compute_instance_convex_decomposition(instance);

    Solution solution(instance);
    for (BinPos bin_pos = 0;
            bin_pos < initial_solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = initial_solution.bin(bin_pos);
        Solution new_solution = ::linear_programming(
                instance,
                initial_solution,
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
    output.solution_pool.add(solution);

    //instance.write("lp_test.json");
    //initial_solution.write("lp_test_initial.json");
    //solution.write("lp_test_expected.json");
    //exit(1);
    //std::cout << "linear_programming end" << std::endl;
    return output;
}
