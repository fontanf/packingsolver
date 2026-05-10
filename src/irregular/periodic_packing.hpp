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
 * Fully resolved periodic packing with item metadata (item type, angle,
 * mirror) and a scaled bounding box. This is the type stored in item columns
 * and returned by compute_periodic_packings.
 */
struct PeriodicItemPacking
{
    std::vector<SolutionItem> items;

    Point vector_1 = {0, 0};

    Point vector_2 = {0, 0};

    /** Axis-aligned bounding box of all items in one cell (scaled coordinates). */
    AxisAlignedBoundingBox aabb_scaled;
};

std::vector<PeriodicItemPacking> compute_periodic_packings(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations);

}
}
