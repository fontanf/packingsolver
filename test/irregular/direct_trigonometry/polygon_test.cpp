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
