#include "irregular/direct_trigonometry/polygon.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

Polygon build_polygon_from_points(const std::vector<Point>& points)
{
    Shape shape;
    for (ElementPos point_pos = 0; point_pos < (ElementPos)points.size(); ++point_pos) {
        ShapeElement element;
        element.type = ShapeElementType::LineSegment;
        element.start = points[point_pos];
        element.end = points[(point_pos + 1) % points.size()];
        shape.elements.push_back(element);
    }
    return build_polygon(shape);
}

Polygon build_rectangle_polygon(
        LengthDbl x_min,
        LengthDbl x_max,
        LengthDbl y_min,
        LengthDbl y_max)
{
    return build_polygon_from_points({
            {x_min, y_min},
            {x_max, y_min},
            {x_max, y_max},
            {x_min, y_max}});
}

/**
 * Ring sector between the radii 'radius_inner' and 'radius_outer' and the
 * angles (in degrees) 'angle_start' and 'angle_end', each arc approximated
 * with 'number_of_segments' segments: a thin item with many edges, so that
 * several chunks of edges are involved.
 */
Polygon build_ring_sector_polygon(
        LengthDbl radius_inner,
        LengthDbl radius_outer,
        Angle angle_start,
        Angle angle_end,
        ElementPos number_of_segments)
{
    std::vector<Point> points;
    for (ElementPos point_pos = 0; point_pos <= number_of_segments; ++point_pos) {
        Angle angle = (angle_start + (angle_end - angle_start) * point_pos / number_of_segments) * M_PI / 180;
        points.push_back({radius_outer * std::cos(angle), radius_outer * std::sin(angle)});
    }
    for (ElementPos point_pos = number_of_segments; point_pos >= 0; --point_pos) {
        Angle angle = (angle_start + (angle_end - angle_start) * point_pos / number_of_segments) * M_PI / 180;
        points.push_back({radius_inner * std::cos(angle), radius_inner * std::sin(angle)});
    }
    return build_polygon_from_points(points);
}

const Point origin = {0.0, 0.0};

}

TEST(IrregularPolygon, LocatePoint)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_EQ(locate_point({1, 1}, square), PointLocation::Inside);
    EXPECT_EQ(locate_point({2, 1}, square), PointLocation::Boundary);
    EXPECT_EQ(locate_point({0, 0}, square), PointLocation::Boundary);
    EXPECT_EQ(locate_point({3, 1}, square), PointLocation::Outside);
}

TEST(IrregularPolygon, InteriorPoint)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_EQ(locate_point(square.interior_point, square), PointLocation::Inside);
}

TEST(IrregularPolygon, InteriorPointNonConvex)
{
    // The triangle formed by the lowest-leftmost vertex (0, 0) and its
    // neighbors (10, 0) and (0, 10) contains the vertices of the notch.
    std::vector<std::vector<Point>> polygons_points = {
        {{0, 0}, {10, 0}, {10, 10}, {6, 10}, {6, 1}, {4, 1}, {4, 10}, {0, 10}},
        {{0, 0}, {10, 0}, {1, 1}, {0, 10}},
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}},
    };
    for (const std::vector<Point>& points: polygons_points) {
        Polygon polygon = build_polygon_from_points(points);
        EXPECT_EQ(locate_point(polygon.interior_point, polygon), PointLocation::Inside);
    }
}

TEST(IrregularPolygon, IntersectDisjoint)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_FALSE(intersect(square, origin, square, {3, 0}));
}

TEST(IrregularPolygon, IntersectTouchingEdge)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_FALSE(intersect(square, origin, square, {2, 0}));
    EXPECT_FALSE(intersect(square, origin, square, {2, 1}));
}

TEST(IrregularPolygon, IntersectTouchingCorner)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_FALSE(intersect(square, origin, square, {2, 2}));
}

TEST(IrregularPolygon, IntersectOverlapping)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_TRUE(intersect(square, origin, square, {1, 1}));
}

TEST(IrregularPolygon, IntersectOverlappingTranslations)
{
    // Both polygons translated: only the relative translation matters.
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_TRUE(intersect(square, {10, 10}, square, {11, 11}));
    EXPECT_FALSE(intersect(square, {10, 10}, square, {12, 10}));
}

TEST(IrregularPolygon, IntersectAlignedRectangles)
{
    // No proper crossing, every vertex and the interior point of each
    // rectangle on the boundary of the other or outside.
    Polygon rectangle = build_rectangle_polygon(0, 2, 0, 1);
    EXPECT_TRUE(intersect(rectangle, origin, rectangle, {1, 0}));
}

TEST(IrregularPolygon, IntersectIdentical)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    EXPECT_TRUE(intersect(square, origin, square, origin));
}

TEST(IrregularPolygon, IntersectContained)
{
    Polygon square_large = build_rectangle_polygon(0, 4, 0, 4);
    Polygon square_small = build_rectangle_polygon(0, 1, 0, 1);
    EXPECT_TRUE(intersect(square_large, origin, square_small, {1, 1}));
    EXPECT_TRUE(intersect(square_small, {1, 1}, square_large, origin));
}

TEST(IrregularPolygon, IntersectContainedTouchingBoundary)
{
    Polygon square_large = build_rectangle_polygon(0, 4, 0, 4);
    Polygon square_small = build_rectangle_polygon(0, 1, 0, 1);
    EXPECT_TRUE(intersect(square_large, origin, square_small, origin));
}

TEST(IrregularPolygon, IntersectCrossingAtVertices)
{
    // A diamond whose edges only meet the square's edges at vertices.
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Polygon diamond = build_polygon_from_points({{1, 1}, {2, 0}, {3, 1}, {2, 2}});
    EXPECT_TRUE(intersect(square, origin, diamond, origin));
    EXPECT_TRUE(intersect(diamond, origin, square, origin));
}

TEST(IrregularPolygon, IntersectNotch)
{
    // A square in the notch of an L-shape, touching it.
    Polygon l_shape = build_polygon_from_points({
            {0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}});
    Polygon square = build_rectangle_polygon(0, 1, 0, 1);
    EXPECT_FALSE(intersect(l_shape, origin, square, {1, 1}));
    EXPECT_TRUE(intersect(l_shape, origin, square, {0.9, 0.9}));
}

TEST(IrregularPolygon, LocatePointRingSector)
{
    Polygon ring_sector = build_ring_sector_polygon(100, 110, 0, 90, 40);
    Angle angle = 45 * M_PI / 180;
    EXPECT_EQ(locate_point({105 * std::cos(angle), 105 * std::sin(angle)}, ring_sector), PointLocation::Inside);
    EXPECT_EQ(locate_point({95 * std::cos(angle), 95 * std::sin(angle)}, ring_sector), PointLocation::Outside);
    EXPECT_EQ(locate_point({115 * std::cos(angle), 115 * std::sin(angle)}, ring_sector), PointLocation::Outside);
    EXPECT_EQ(locate_point({110, 0}, ring_sector), PointLocation::Boundary);
}

TEST(IrregularPolygon, IntersectRingSectorsNested)
{
    // Nested ring sectors with the same angles: separated, touching or
    // overlapping.
    Polygon ring_sector = build_ring_sector_polygon(100, 110, 0, 90, 40);
    Polygon ring_sector_separated = build_ring_sector_polygon(111, 121, 0, 90, 40);
    Polygon ring_sector_touching = build_ring_sector_polygon(110, 120, 0, 90, 40);
    Polygon ring_sector_overlapping = build_ring_sector_polygon(105, 115, 0, 90, 40);
    for (const Polygon* polygon: {&ring_sector_separated, &ring_sector_touching}) {
        EXPECT_FALSE(intersect(ring_sector, origin, *polygon, origin));
        EXPECT_FALSE(intersect(*polygon, origin, ring_sector, origin));
    }
    EXPECT_TRUE(intersect(ring_sector, origin, ring_sector_overlapping, origin));
    EXPECT_TRUE(intersect(ring_sector_overlapping, origin, ring_sector, origin));
}

TEST(IrregularPolygon, IntersectRingSectorsShifted)
{
    // Only the ends of the ring sectors overlap.
    Polygon ring_sector_1 = build_ring_sector_polygon(100, 110, 0, 90, 40);
    Polygon ring_sector_2 = build_ring_sector_polygon(100, 110, 80, 170, 40);
    EXPECT_TRUE(intersect(ring_sector_1, origin, ring_sector_2, origin));
    EXPECT_TRUE(intersect(ring_sector_2, origin, ring_sector_1, origin));
    Polygon ring_sector_3 = build_ring_sector_polygon(100, 110, 91, 180, 40);
    EXPECT_FALSE(intersect(ring_sector_1, origin, ring_sector_3, origin));
    EXPECT_FALSE(intersect(ring_sector_3, origin, ring_sector_1, origin));
}

TEST(IrregularPolygon, IntersectRingSectorContained)
{
    // A small square inside a thick ring sector, and a square in the
    // concave side of the ring sector, inside its convex hull.
    Polygon ring_sector = build_ring_sector_polygon(100, 130, 0, 90, 40);
    Polygon square = build_rectangle_polygon(-2, 2, -2, 2);
    Angle angle = 45 * M_PI / 180;
    Point inside = {115 * std::cos(angle), 115 * std::sin(angle)};
    EXPECT_TRUE(intersect(ring_sector, origin, square, inside));
    EXPECT_TRUE(intersect(square, inside, ring_sector, origin));
    Point concave_side = {80 * std::cos(angle), 80 * std::sin(angle)};
    EXPECT_FALSE(intersect(ring_sector, origin, square, concave_side));
    EXPECT_FALSE(intersect(square, concave_side, ring_sector, origin));
}

TEST(IrregularPolygon, IntersectCircleDisjoint)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Pole circle = {{0, 0}, 1};
    EXPECT_FALSE(intersect(circle, {4, 1}, square, origin));
    // Outside the square, but inside its bounding box enlarged by the radius.
    EXPECT_FALSE(intersect(circle, {2.8, 2.8}, square, origin));
}

TEST(IrregularPolygon, IntersectCircleTouchingEdge)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Pole circle = {{0, 0}, 1};
    EXPECT_FALSE(intersect(circle, {3, 1}, square, origin));
    EXPECT_FALSE(intersect(circle, {1, -1}, square, origin));
}

TEST(IrregularPolygon, IntersectCircleTouchingCorner)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Pole circle = {{0, 0}, 1};
    EXPECT_FALSE(intersect(circle, {2 + std::sqrt(0.5), 2 + std::sqrt(0.5)}, square, origin));
    EXPECT_TRUE(intersect(circle, {2.7, 2.7}, square, origin));
}

TEST(IrregularPolygon, IntersectCircleOverlappingEdge)
{
    // The center is outside the square.
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Pole circle = {{0, 0}, 1};
    EXPECT_TRUE(intersect(circle, {2.9, 1}, square, origin));
}

TEST(IrregularPolygon, IntersectCircleCenterInside)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Pole circle = {{0, 0}, 0.5};
    EXPECT_TRUE(intersect(circle, {1, 1}, square, origin));
}

TEST(IrregularPolygon, IntersectCircleContainingPolygon)
{
    // The center is outside the triangle, which is inside the circle.
    Polygon triangle = build_polygon_from_points({{1, 0}, {2, 0}, {1, 1}});
    Pole circle = {{0, 0}, 5};
    EXPECT_TRUE(intersect(circle, origin, triangle, origin));
}

TEST(IrregularPolygon, IntersectCircleNotch)
{
    // A circle in the notch of an L-shape, touching both of its sides.
    Polygon l_shape = build_polygon_from_points({
            {0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}});
    Pole circle = {{0, 0}, 0.5};
    EXPECT_FALSE(intersect(circle, {1.5, 1.5}, l_shape, origin));
    EXPECT_TRUE(intersect(circle, {1.4, 1.5}, l_shape, origin));
}

TEST(IrregularPolygon, IntersectCircleTranslations)
{
    // Both shapes translated: only the relative translation matters.
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    Pole circle = {{1, 1}, 1};
    EXPECT_FALSE(intersect(circle, {12, 10}, square, {10, 10}));
    EXPECT_TRUE(intersect(circle, {11.9, 10}, square, {10, 10}));
}

TEST(IrregularPolygon, Poles)
{
    Polygon square = build_rectangle_polygon(0, 2, 0, 2);
    std::vector<Pole> poles = compute_poles(square);
    ASSERT_FALSE(poles.empty());
    EXPECT_NEAR(poles[0].center.x, 1, 1e-2);
    EXPECT_NEAR(poles[0].center.y, 1, 1e-2);
    EXPECT_NEAR(poles[0].radius, 1, 1e-2);
    for (Counter pole_pos_1 = 0; pole_pos_1 < (Counter)poles.size(); ++pole_pos_1) {
        const Pole& pole_1 = poles[pole_pos_1];
        // Inside the square.
        EXPECT_GE(pole_1.center.x - pole_1.radius, -1e-6);
        EXPECT_LE(pole_1.center.x + pole_1.radius, 2 + 1e-6);
        EXPECT_GE(pole_1.center.y - pole_1.radius, -1e-6);
        EXPECT_LE(pole_1.center.y + pole_1.radius, 2 + 1e-6);
        // Disjoint from the other poles.
        for (Counter pole_pos_2 = pole_pos_1 + 1; pole_pos_2 < (Counter)poles.size(); ++pole_pos_2) {
            const Pole& pole_2 = poles[pole_pos_2];
            LengthDbl distance = std::sqrt(
                    (pole_1.center.x - pole_2.center.x) * (pole_1.center.x - pole_2.center.x)
                    + (pole_1.center.y - pole_2.center.y) * (pole_1.center.y - pole_2.center.y));
            EXPECT_GE(distance, pole_1.radius + pole_2.radius - 1e-6);
        }
    }
}
