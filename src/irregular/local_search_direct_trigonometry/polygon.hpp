/**
 * Polygon geometry for the overlap minimization local search.
 *
 * The local search evaluates a very large number of candidate placements, so
 * it works on plain polygons (the simplified shapes of the instance, whose
 * elements are line segments) and on translations only: the rotations are
 * discrete and applied once, when the polygons are built.
 */

#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/** A circle inscribed in a polygon. */
struct Pole
{
    /** Center. */
    Point center = {0.0, 0.0};

    /** Radius. */
    LengthDbl radius = 0.0;
};

/** A simple polygon. */
struct Polygon
{
    /** Vertices. */
    std::vector<Point> vertices;

    /** Length of each edge (from a vertex to the next one). */
    std::vector<LengthDbl> edge_lengths;

    /** Convex hull, anticlockwise. */
    std::vector<Point> convex_hull;

    /** Bounding box. */
    AxisAlignedBoundingBox aabb;

    /** A point strictly inside the polygon. */
    Point interior_point = {0.0, 0.0};
};

/**
 * Build a polygon from the outer boundary of a shape whose elements are line
 * segments.
 *
 * The interior point is the center of the pole of inaccessibility.
 */
Polygon build_polygon(const Shape& shape);

/** Location of a point relatively to a polygon. */
enum class PointLocation
{
    Outside,
    Boundary,
    Inside,
};

/** Locate 'point' relatively to 'polygon'. */
PointLocation locate_point(
        const Point& point,
        const Polygon& polygon);

/**
 * Return true iff the interiors of 'polygon_1' translated by 'translation_1'
 * and of 'polygon_2' translated by 'translation_2' intersect.
 *
 * Polygons which only touch (sharing boundary points or boundary segments)
 * don't intersect.
 */
bool intersect(
        const Polygon& polygon_1,
        const Point& translation_1,
        const Polygon& polygon_2,
        const Point& translation_2);

/**
 * Compute the poles of a polygon: disjoint circles inscribed in it, the first
 * one being the largest one (the pole of inaccessibility), each next one the
 * largest one inscribed in the polygon minus the previous ones.
 *
 * Poles are generated until 'maximum_number_of_poles' poles have been
 * generated, or until the poles cover a large enough part of the polygon:
 * 90% with at least 8 poles, 80% with at least 16 poles.
 */
std::vector<Pole> compute_poles(
        const Polygon& polygon,
        Counter maximum_number_of_poles = 64);

}
}
