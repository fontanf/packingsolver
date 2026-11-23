#pragma once

#include "packingsolver/irregular/instance.hpp"

#include "algorithms/leftover.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * Get the leftover value of a bin, given the bounding box of the bin and the
 * bounding box of its items (see 'LeftoverMode'), for a leftover value type.
 *
 * With 'LeftoverValueType::SquaredArea', the value of a leftover is the
 * square of its area (the sum of the squares of the areas of its pieces if it
 * is cut). With
 * 'LeftoverValueType::Depth', it is the integral, over its points, of their
 * distance to its edge (see 'algorithms/leftover.hpp'): its area times the
 * average distance of its points to its edge, so a compact leftover is worth
 * more than a thin one of the same area.
 */
double leftover_value(
        LeftoverValueType leftover_value_type,
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb);

/**
 * Get the area of the leftover of a bin, given the bounding box of the bin
 * and the bounding box of its items (see 'LeftoverMode'): the area of the
 * bounding box of the bin minus the area of the rectangle used by the items.
 */
AreaDbl leftover_area(
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb);

/**
 * Get the leftover value of an empty bin, given its bounding box, for a
 * leftover value type.
 */
double leftover_value(
        LeftoverValueType leftover_value_type,
        const AxisAlignedBoundingBox& bin_aabb);

/**
 * Staircase of the items of a bin.
 *
 * An item dominates the points of the rectangle between the corner of the
 * leftover mode and the opposite corner of its bounding box. The staircase
 * leftover value of a bin is the area of the bin which is not dominated by an
 * item.
 */
class Staircase
{

public:

    /** Add an item, given the bounding box of the bin and of the item. */
    void add_item(
            LeftoverMode leftover_mode,
            const AxisAlignedBoundingBox& bin_aabb,
            const AxisAlignedBoundingBox& item_aabb);

    /** Get the area dominated by the items. */
    AreaDbl dominated_area() const;

    /** Get the staircase leftover value of a bin. */
    AreaDbl leftover_value(const AxisAlignedBoundingBox& bin_aabb) const;

    /**
     * Get the value of the staircase leftover of a bin, for a leftover value
     * type (for the leftover modes ending with 'Staircase').
     */
    double value(
            LeftoverValueType leftover_value_type,
            const AxisAlignedBoundingBox& bin_aabb) const;

private:

    /**
     * Non-dominated distances, along x and y, between the corner of the
     * leftover mode and the opposite corner of the bounding box of the items,
     * by decreasing distance along x, and therefore increasing distance along
     * y.
     */
    std::vector<std::pair<LengthDbl, LengthDbl>> extents_;

};

}
}
