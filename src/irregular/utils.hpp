/**
 * Private helpers shared between linear_programming.cpp and
 * local_nonlinear_programming.cpp.
 *
 * All functions are inline to allow inclusion from multiple translation units.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "shape/convex_partition.hpp"
#include "shape/intersection_tree.hpp"

#include <vector>

namespace packingsolver
{
namespace irregular
{

////////////////////////////////////////////////////////////////////////////////
// Convex decomposition of the instance
////////////////////////////////////////////////////////////////////////////////

struct InstanceConvexDecomposition
{
    std::vector<std::vector<std::vector<Shape>>> item_types;
    std::vector<std::vector<Shape>> bin_types_borders;
    std::vector<std::vector<Shape>> bin_types_defects;
};

inline InstanceConvexDecomposition compute_instance_convex_decomposition(
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
        const std::vector<std::vector<std::vector<Shape>>>& item_rotated_decompositions,
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
                const AxisAlignedBoundingBox p = parts[part_pos].compute_min_max();

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

}  // namespace irregular
}  // namespace packingsolver
