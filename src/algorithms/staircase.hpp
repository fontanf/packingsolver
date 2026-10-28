#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace packingsolver
{

/**
 * Union of rectangles [0, x] x [0, y].
 *
 * It is used to compute the staircase leftover value of a bin: an item
 * dominates the points of the rectangle between the corner of the bin and the
 * opposite corner of the item, and the staircase leftover value is the area
 * of the bin which is not dominated by an item.
 */
class Staircase2D
{

public:

    /** Add a rectangle [0, x] x [0, y]. */
    void add(
            Length x,
            Length y)
    {
        // Check if the new extent is dominated.
        for (const auto& extent: extents_)
            if (extent[0] >= x && extent[1] >= y)
                return;

        // Remove the extents dominated by the new one, and insert it.
        std::vector<std::array<Length, 2>> extents;
        extents.reserve(extents_.size() + 1);
        bool inserted = false;
        for (const auto& extent: extents_) {
            if (extent[0] <= x && extent[1] <= y)
                continue;
            if (!inserted && extent[0] < x) {
                extents.push_back({x, y});
                inserted = true;
            }
            extents.push_back(extent);
        }
        if (!inserted)
            extents.push_back({x, y});
        extents_.swap(extents);

        // Update the area.
        area_ = 0;
        for (ItemPos pos = 0; pos < (ItemPos)extents_.size(); ++pos) {
            Length next_x = (pos + 1 < (ItemPos)extents_.size())?
                extents_[pos + 1][0]: 0;
            area_ += (extents_[pos][0] - next_x) * extents_[pos][1];
        }
    }

    /** Get the area of the union of the rectangles. */
    inline Area area() const { return area_; }

private:

    /**
     * Non-dominated extents, by decreasing x, and therefore increasing y.
     */
    std::vector<std::array<Length, 2>> extents_;

    /** Area of the union of the rectangles. */
    Area area_ = 0;

};

/**
 * Union of boxes [0, x] x [0, y] x [0, z].
 *
 * It is the three-dimensional counterpart of 'Staircase2D'.
 */
class Staircase3D
{

public:

    /** Add a box [0, x] x [0, y] x [0, z]. */
    void add(
            Length x,
            Length y,
            Length z)
    {
        // Check if the new extent is dominated.
        for (const auto& extent: extents_)
            if (extent[0] >= x && extent[1] >= y && extent[2] >= z)
                return;

        // Remove the extents dominated by the new one, and insert it.
        std::vector<std::array<Length, 3>> extents;
        extents.reserve(extents_.size() + 1);
        for (const auto& extent: extents_)
            if (extent[0] > x || extent[1] > y || extent[2] > z)
                extents.push_back(extent);
        extents.push_back({x, y, z});
        extents_.swap(extents);

        // Update the volume, by decreasing z: between two consecutive values
        // of z, the section of the union is the union of the rectangles of
        // the extents whose z is greater.
        std::vector<std::array<Length, 3>> sorted_extents = extents_;
        std::sort(
                sorted_extents.begin(),
                sorted_extents.end(),
                [](
                    const std::array<Length, 3>& extent_1,
                    const std::array<Length, 3>& extent_2)
                {
                    return extent_1[2] > extent_2[2];
                });
        volume_ = 0;
        Staircase2D section;
        for (ItemPos pos = 0; pos < (ItemPos)sorted_extents.size(); ++pos) {
            section.add(sorted_extents[pos][0], sorted_extents[pos][1]);
            Length next_z = (pos + 1 < (ItemPos)sorted_extents.size())?
                sorted_extents[pos + 1][2]: 0;
            volume_ += section.area() * (sorted_extents[pos][2] - next_z);
        }
    }

    /** Get the volume of the union of the boxes. */
    inline Volume volume() const { return volume_; }

private:

    /** Non-dominated extents. */
    std::vector<std::array<Length, 3>> extents_;

    /** Volume of the union of the boxes. */
    Volume volume_ = 0;

};

}
