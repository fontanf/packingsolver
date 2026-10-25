/**
 * No-fit polygons and penetration depths for the black-box optimization
 * engine of the local search.
 *
 * The no-fit polygon of a fixed item A and of an orbiting item B is the set of
 * the positions of the reference point of B, relatively to the reference point
 * of A, for which the two items overlap. The penetration depth of B into A is
 * the distance from the relative position of B to the boundary of the no-fit
 * polygon, when it is inside: the length of the smallest translation of B
 * which separates the items.
 *
 * The no-fit polygons are polygons with holes, possibly with several
 * components; a hole is a pocket of positions without overlap (e.g. B inside
 * a hole of A).
 */

#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/** No-fit polygon, prepared for signed distance queries. */
struct NoFitPolygon
{
    /** Boundary ring of a component: its outline or one of its holes. */
    struct Ring
    {
        /** Vertices. */
        std::vector<Point> vertices;

        /** Bounding box. */
        AxisAlignedBoundingBox aabb;

        /**
         * True iff the interior of the no-fit polygon is on the left of the
         * edges of the ring.
         */
        bool interior_on_left = true;
    };

    /** Component: an outline and its holes. */
    struct Component
    {
        /** Outline (first ring) and holes (next rings). */
        std::vector<Ring> rings;
    };

    /** Components. */
    std::vector<Component> components;

    /** Bounding box. */
    AxisAlignedBoundingBox aabb;
};

/**
 * Build the no-fit polygon of a fixed item and of an orbiting item, given by
 * their shapes, relatively to their reference points.
 *
 * It is the union of the no-fit polygons of the pairs of shapes. The circular
 * arcs, if any, are approximated by line segments.
 */
NoFitPolygon build_no_fit_polygon(
        const std::vector<ShapeWithHoles>& fixed_shapes,
        const std::vector<ShapeWithHoles>& orbiting_shapes);

/** Build a no-fit polygon from its shapes. */
NoFitPolygon build_no_fit_polygon(const MultiShapeWithHoles& shapes);

/** Signed distance from a point to the boundary of a no-fit polygon. */
struct SignedDistance
{
    /**
     * Signed distance: positive inside the no-fit polygon (the penetration
     * depth), negative outside.
     */
    LengthDbl distance = 0.0;

    /** Gradient of the signed distance with respect to the point. */
    Point gradient = {0.0, 0.0};
};

/**
 * Compute the signed distance from a point to the boundary of a no-fit
 * polygon.
 *
 * Outside the no-fit polygon, only the distances smaller than
 * 'maximum_outside_distance' are computed: return false if the point is
 * outside, farther from the no-fit polygon than this distance.
 */
bool signed_distance(
        const NoFitPolygon& no_fit_polygon,
        const Point& point,
        LengthDbl maximum_outside_distance,
        SignedDistance& output);

}
}
