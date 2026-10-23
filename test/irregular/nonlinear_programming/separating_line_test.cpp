#include "irregular/nonlinear_programming/separating_line.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/** Gap between two polygons along a direction. */
LengthDbl gap(
        const std::vector<Point>& polygon_1,
        const std::vector<Point>& polygon_2,
        const Point& normal)
{
    LengthDbl maximum_1 = -std::numeric_limits<LengthDbl>::infinity();
    for (const Point& vertex: polygon_1)
        maximum_1 = (std::max)(maximum_1, normal.x * vertex.x + normal.y * vertex.y);
    LengthDbl minimum_2 = std::numeric_limits<LengthDbl>::infinity();
    for (const Point& vertex: polygon_2)
        minimum_2 = (std::min)(minimum_2, normal.x * vertex.x + normal.y * vertex.y);
    return minimum_2 - maximum_1;
}

/** Largest gap over a sample of directions. */
LengthDbl brute_force_gap(
        const std::vector<Point>& polygon_1,
        const std::vector<Point>& polygon_2)
{
    const Counter number_of_directions = 36000;
    LengthDbl best_gap = -std::numeric_limits<LengthDbl>::infinity();
    for (Counter direction_pos = 0; direction_pos < number_of_directions; ++direction_pos) {
        Angle angle = 2 * M_PI * direction_pos / number_of_directions;
        best_gap = (std::max)(best_gap, gap(polygon_1, polygon_2, {std::cos(angle), std::sin(angle)}));
    }
    return best_gap;
}

/** Random convex polygon, anticlockwise. */
std::vector<Point> random_convex_polygon(
        std::mt19937_64& generator,
        const Point& center,
        LengthDbl radius)
{
    std::uniform_real_distribution<Angle> angle_distribution(0, 2 * M_PI);
    std::uniform_int_distribution<Counter> size_distribution(3, 9);
    std::vector<Angle> angles(size_distribution(generator));
    for (Angle& angle: angles)
        angle = angle_distribution(generator);
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end()), angles.end());
    std::vector<Point> polygon;
    for (Angle angle: angles)
        polygon.push_back({center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
    return polygon;
}

/** Check a separating line against a brute force search. */
void check(
        const std::vector<Point>& polygon_1,
        const std::vector<Point>& polygon_2)
{
    SeparatingLine separating_line = compute_separating_line(polygon_1, polygon_2);
    EXPECT_NEAR(std::hypot(separating_line.normal.x, separating_line.normal.y), 1.0, 1e-9);
    EXPECT_NEAR(gap(polygon_1, polygon_2, separating_line.normal), separating_line.gap, 1e-9);
    EXPECT_GE(separating_line.gap, brute_force_gap(polygon_1, polygon_2) - 1e-9);
}

/** Unit square whose bottom-left corner is 'corner'. */
std::vector<Point> square(
        LengthDbl corner_x,
        LengthDbl corner_y)
{
    return {
        {corner_x, corner_y},
        {corner_x + 1, corner_y},
        {corner_x + 1, corner_y + 1},
        {corner_x, corner_y + 1}};
}

}

TEST(IrregularSeparatingLine, CornerToCorner)
{
    // The best separating line of two squares placed corner to corner is
    // diagonal: it isn't an edge normal.
    std::vector<Point> square_1 = square(0, 0);
    std::vector<Point> square_2 = square(2, 2);
    SeparatingLine separating_line = compute_separating_line(square_1, square_2);
    EXPECT_NEAR(separating_line.normal.x, std::sqrt(0.5), 1e-9);
    EXPECT_NEAR(separating_line.normal.y, std::sqrt(0.5), 1e-9);
    EXPECT_NEAR(separating_line.gap, std::sqrt(2.0), 1e-9);
    EXPECT_NEAR(separating_line.offset, 1.5 * std::sqrt(2.0), 1e-9);
}

TEST(IrregularSeparatingLine, Overlapping)
{
    // Two squares overlapping by 0.25 along x.
    SeparatingLine separating_line = compute_separating_line(square(0, 0), square(0.75, 0.5));
    EXPECT_NEAR(separating_line.normal.x, 1.0, 1e-9);
    EXPECT_NEAR(separating_line.normal.y, 0.0, 1e-9);
    EXPECT_NEAR(separating_line.gap, -0.25, 1e-9);
}

TEST(IrregularSeparatingLine, Touching)
{
    // A corner of the second square on the hypotenuse of the triangle.
    std::vector<Point> triangle = {{0, 0}, {4, 0}, {0, 4}};
    check(triangle, square(2, 2));
    EXPECT_NEAR(compute_separating_line(triangle, square(2, 2)).gap, 0.0, 1e-9);
}

TEST(IrregularSeparatingLine, Points)
{
    // Polygons reduced to points, as the parts of the items with a null
    // scale.
    std::vector<Point> point_1 = {{1, 1}, {1, 1}, {1, 1}};
    std::vector<Point> point_2 = {{-2, 5}, {-2, 5}, {-2, 5}, {-2, 5}};
    SeparatingLine separating_line = compute_separating_line(point_1, point_2);
    EXPECT_NEAR(separating_line.normal.x, -0.6, 1e-9);
    EXPECT_NEAR(separating_line.normal.y, 0.8, 1e-9);
    EXPECT_NEAR(separating_line.gap, 5.0, 1e-9);
    check(point_1, square(3, 0));
}

TEST(IrregularSeparatingLine, Random)
{
    // Random convex polygons, possibly reduced to a point, to a segment, or
    // shrunk to almost a point.
    std::mt19937_64 generator(0);
    std::uniform_real_distribution<LengthDbl> distribution(-3, 3);
    for (Counter test_pos = 0; test_pos < 300; ++test_pos) {
        std::vector<Point> polygon_1 = random_convex_polygon(generator, {0, 0}, 1);
        std::vector<Point> polygon_2 = random_convex_polygon(
                generator,
                {distribution(generator), distribution(generator)},
                1.5);
        switch (test_pos % 5) {
        case 1: {
            // Point.
            for (Point& vertex: polygon_1)
                vertex = polygon_1[0];
            break;
        } case 2: {
            // Segment.
            for (Point& vertex: polygon_1)
                vertex.y = polygon_1[0].y;
            break;
        } case 3: {
            // Almost a point.
            Point origin = polygon_2[0];
            for (Point& vertex: polygon_2) {
                vertex.x = origin.x + 1e-15 * (vertex.x - origin.x);
                vertex.y = origin.y + 1e-15 * (vertex.y - origin.y);
            }
            break;
        } case 4: {
            // Touching: translate the second polygon by its gap.
            SeparatingLine separating_line = compute_separating_line(polygon_1, polygon_2);
            for (Point& vertex: polygon_2) {
                vertex.x -= separating_line.gap * separating_line.normal.x;
                vertex.y -= separating_line.gap * separating_line.normal.y;
            }
            break;
        } default: {
            break;
        }
        }
        check(polygon_1, polygon_2);
    }
}
