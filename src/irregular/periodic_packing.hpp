#pragma once

#include "packingsolver/irregular/solution.hpp"
#include "irregular/rotations.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * Geometric result of a periodic packing computation.
 *
 * positions[i] is the placement of the i-th shape in the unit cell.
 * An empty positions vector signals that no valid packing was found.
 */
struct PeriodicPacking
{
    std::vector<Point> positions;

    Point vector_1 = {0, 0};

    Point vector_2 = {0, 0};
};

std::ostream& operator<<(
        std::ostream& os,
        const PeriodicPacking& periodic_packing);

bool equal(
        const PeriodicPacking& packing_1,
        const PeriodicPacking& packing_2);

std::vector<PeriodicPacking> compute_periodic_packings(
        const ShapeWithHoles& shape,
        LengthDbl item_item_minimum_spacing = 0.0);

std::vector<PeriodicPacking> compute_periodic_packings(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_r,
        LengthDbl item_item_minimum_spacing = 0.0);

/**
 * Compute periodic packings for a single item type (self-pairing and, when
 * allowed, pairing a rotation with its r + 180 counterpart).
 *
 * Only depends on the item's own (unrotated) sub-shapes and allowed
 * rotations, not on any other item type or on bin dimensions -- hence
 * 'item_shapes' rather than an (Instance, ItemTypeId) pair, so this can be
 * called directly on e.g. a simplified version of an item type's shapes
 * without needing to build a full Instance around them.
 *
 * The returned PeriodicItemPacking::items do not have item_type_id set
 * (this function has no item type identity to give them); the caller is
 * responsible for filling it in.
 */
std::vector<PeriodicItemPacking> compute_periodic_packings_for_item_type(
        const std::vector<ShapeWithHoles>& item_shapes,
        const std::vector<ItemTypeRotation>& rotations,
        LengthDbl item_item_minimum_spacing = 0.0);

std::vector<PeriodicItemPacking> compute_periodic_packings(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations);

/** Side of an axis-aligned bounding box. */
enum class BoundingBoxSide
{
    Bottom,
    Top,
    Left,
    Right,
};

/**
 * Compute the largest empty rectangle inside the axis-aligned bounding box
 * of a shape, touching a given side of the bounding box.
 *
 * It measures how much a shape could wrap around other items: for an
 * L-shape, the rectangle between its two arms touches the top and the right
 * sides; for a U-shape, its cavity touches the side it opens on. For a
 * convex shape, the rectangle is small.
 *
 * Only the outline of the shape is considered (its holes are not empty space
 * touching a side of the bounding box). Circular arcs are approximated by
 * line segments outside of the shape, with segments of length
 * 'segment_length', so that the rectangle returned is always empty; the
 * bounding box is the one of this approximation. Between two consecutive
 * vertex abscissas (along the side), the profile of the shape is replaced by
 * its farthest point from the side, so the rectangle returned may be
 * slightly smaller than the largest one.
 *
 * Return an empty bounding box (x_min > x_max) if there is no such
 * rectangle of positive area.
 */
AxisAlignedBoundingBox compute_largest_empty_side_rectangle(
        const ShapeWithHoles& shape,
        BoundingBoxSide side,
        LengthDbl segment_length);

/** Return true iff all the items of 'packing' have the same item type. */
bool periodic_packing_has_single_item_type(
        const PeriodicItemPacking& packing);

/**
 * Get the combined shape of an item type for a given rotation.
 *
 * Applies mirror (if requested) then rotation angle to each sub-shape, then
 * returns their union. If the item has a single sub-shape, returns it directly.
 */
ShapeWithHoles get_item_combined_shape(
        const Instance& instance,
        ItemTypeId item_type_id,
        const ItemTypeRotation& rotation);

}
}
