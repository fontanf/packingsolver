#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * A maximal empty space: the largest axis-aligned rectangle that can be placed
 * at a given position without overlapping any placed block or the bin walls.
 * All lengths are in scaled coordinates.
 */
struct EmptySpace
{
    Point bl_corner = {0.0, 0.0};
    LengthDbl bx = 0.0;
    LengthDbl by = 0.0;

    inline LengthDbl xs() const { return bl_corner.x; }
    inline LengthDbl ys() const { return bl_corner.y; }
    inline LengthDbl xe() const { return bl_corner.x + bx; }
    inline LengthDbl ye() const { return bl_corner.y + by; }

    /** Returns true iff this space fully contains other. */
    inline bool contains(const EmptySpace& other) const
    {
        return !strictly_greater(xs(), other.xs())
            && !strictly_greater(ys(), other.ys())
            && !strictly_lesser(xe(), other.xe())
            && !strictly_lesser(ye(), other.ye());
    }

    inline AreaDbl area() const { return bx * by; }
};

/**
 * Return 'true' iff the rectangle of bottom-left corner 'bl_corner' and of
 * dimensions 'bx' x 'by' overlaps the empty space.
 */
bool overlaps(
        const EmptySpace& space,
        Point bl_corner,
        LengthDbl bx,
        LengthDbl by);

/**
 * Add an empty space to a list of maximal empty spaces, unless it is empty or
 * contained in one of them.
 */
void add_empty_space(
        std::vector<EmptySpace>& spaces,
        const EmptySpace& new_space);

/**
 * Update a list of maximal empty spaces when the rectangle of bottom-left
 * corner 'bl_corner' and of dimensions 'bx' x 'by' gets occupied: each empty
 * space it overlaps is replaced by its (up to four) parts on the left, on the
 * right, below and above the rectangle.
 */
void cut_empty_spaces(
        std::vector<EmptySpace>& spaces,
        Point bl_corner,
        LengthDbl bx,
        LengthDbl by);

}
}
