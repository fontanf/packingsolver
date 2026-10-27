#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * Get the leftover value of a bin, given the bounding box of the bin and the
 * bounding box of its items: the area of the bin minus the area of the
 * rectangle between the corner (or the side) of the leftover mode and the
 * opposite corner (or side) of the bounding box of the items.
 */
AreaDbl leftover_value(
        LeftoverMode leftover_mode,
        const AxisAlignedBoundingBox& bin_aabb,
        const AxisAlignedBoundingBox& items_aabb);

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
