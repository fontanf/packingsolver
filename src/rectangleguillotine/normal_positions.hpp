#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

/*
 * The normal positions along a dimension of the bin: the lengths of the
 * combinations of item dimensions (with a cut thickness between two
 * consecutive items), up to the length of the bin. Any guillotine pattern
 * can be transformed into a pattern with the same items whose cuts are at
 * normal positions (pushing its items towards the origin) (Herz 1972;
 * Christofides and Whitlock 1977).
 */
struct NormalPositions
{
    /** The normal positions, increasing; 'positions[0]' is 0. */
    std::vector<Length> positions;

    /**
     * For each length 'x' between 0 and the length of the bin, the index of
     * the largest normal position not larger than 'x'.
     */
    std::vector<Counter> floor;
};

NormalPositions compute_normal_positions(
        const std::vector<Length>& dimensions,
        Length length,
        Length cut_thickness);

/*
 * The raster points along a dimension of the bin (Scheithauer and Terno,
 * 1996): the lengths of what remains of the bin after cutting a normal
 * length, rounded down to a normal position:
 *   R = { <L - q - t> : q normal position } (and 0),
 * where <x> is the largest normal position not larger than x.
 *
 * They are the only lengths of the sub-rectangles that a dynamic program
 * with an unlimited number of stages needs: a sub-rectangle of a pattern can
 * be extended until the content of its neighbor (pushed to the other side),
 * and its length is then a raster point. If 'x' is a raster point and 'c' a
 * normal position, <x - c - t> is also a raster point.
 */
NormalPositions compute_raster_points(
        const NormalPositions& normal_positions,
        Length length,
        Length cut_thickness);

}
}
