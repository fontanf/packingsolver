/**
 * Separating line of two convex polygons
 *
 * For a unit direction 'u', the gap between two convex polygons A and B along
 * 'u' is min_b u.b - max_a u.a = -h_M(u), where h_M is the support function
 * of their Minkowski difference M = A - B. The best separating line maximizes
 * this gap:
 * - if A and B overlap (the origin is inside M), its normal is the outward
 *   normal of the edge of M closest to the origin; the edges of M being the
 *   edges of A and of -B, it is an edge normal of A or of B;
 * - otherwise, its normal points from the closest point of M to the origin;
 *   it may not be an edge normal of A or of B.
 *
 * M is built in O(n + m) by merging the edges of A and of -B sorted by angle.
 */

#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/** Separating line of two convex polygons. */
struct SeparatingLine
{
    /** Unit normal, oriented from the first polygon to the second one. */
    Point normal = {1.0, 0.0};

    /**
     * Offset: halfway between the two polygons along the normal, i.e.
     * (max_a normal.a + min_b normal.b) / 2.
     */
    LengthDbl offset = 0.0;

    /**
     * Gap between the polygons along the normal:
     * min_b normal.b - max_a normal.a; negative if they overlap.
     */
    LengthDbl gap = 0.0;
};

/**
 * Compute the separating line of two convex polygons maximizing the gap
 * between them.
 *
 * The vertices of the polygons must be given anticlockwise.
 */
SeparatingLine compute_separating_line(
        const std::vector<Point>& polygon_1,
        const std::vector<Point>& polygon_2);

}
}
