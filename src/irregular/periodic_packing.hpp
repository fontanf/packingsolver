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
        const ShapeWithHoles& shape);

std::vector<PeriodicPacking> compute_periodic_packings(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_r);

/**
 * Compute periodic packings for a single item type (self-pairing and, when
 * allowed, pairing a rotation with its r + 180 counterpart).
 *
 * Only depends on the item type's own shape and allowed rotations, not on
 * any other item type or on bin dimensions, so its result is cacheable on
 * ItemType (see ItemType::periodic_packings in instance.hpp).
 */
std::vector<PeriodicItemPacking> compute_periodic_packings_for_item_type(
        const Instance& instance,
        ItemTypeId item_type_id,
        const std::vector<ItemTypeRotation>& rotations);

std::vector<PeriodicItemPacking> compute_periodic_packings(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations);

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
