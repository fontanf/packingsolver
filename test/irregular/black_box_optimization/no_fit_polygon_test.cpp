#include "irregular/black_box_optimization/no_fit_polygon.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

ShapeWithHoles rectangle(
        LengthDbl x_min,
        LengthDbl x_max,
        LengthDbl y_min,
        LengthDbl y_max)
{
    ShapeWithHoles shape;
    shape.shape = shape::build_rectangle(x_min, x_max, y_min, y_max);
    return shape;
}

}

TEST(IrregularBlackBoxOptimizationNoFitPolygon, SquaresInside)
{
    // NFP of two 10x10 squares: the square [-10, 10] x [-10, 10].
    NoFitPolygon no_fit_polygon = build_no_fit_polygon(
            {rectangle(0, 10, 0, 10)},
            {rectangle(0, 10, 0, 10)});
    SignedDistance output;
    ASSERT_TRUE(signed_distance(no_fit_polygon, {2, 0}, 1.0, output));
    EXPECT_NEAR(output.distance, 8, 1e-9);
    EXPECT_NEAR(output.gradient.x, -1, 1e-9);
    EXPECT_NEAR(output.gradient.y, 0, 1e-9);
}

TEST(IrregularBlackBoxOptimizationNoFitPolygon, SquaresOutside)
{
    NoFitPolygon no_fit_polygon = build_no_fit_polygon(
            {rectangle(0, 10, 0, 10)},
            {rectangle(0, 10, 0, 10)});
    SignedDistance output;
    ASSERT_TRUE(signed_distance(no_fit_polygon, {11, 0}, 2.0, output));
    EXPECT_NEAR(output.distance, -1, 1e-9);
    EXPECT_NEAR(output.gradient.x, -1, 1e-9);
    EXPECT_NEAR(output.gradient.y, 0, 1e-9);
    // Farther than the maximum outside distance.
    EXPECT_FALSE(signed_distance(no_fit_polygon, {13, 0}, 2.0, output));
}

TEST(IrregularBlackBoxOptimizationNoFitPolygon, SquaresBoundary)
{
    NoFitPolygon no_fit_polygon = build_no_fit_polygon(
            {rectangle(0, 10, 0, 10)},
            {rectangle(0, 10, 0, 10)});
    SignedDistance output;
    ASSERT_TRUE(signed_distance(no_fit_polygon, {0, 10}, 1.0, output));
    EXPECT_NEAR(output.distance, 0, 1e-9);
    // Towards the interior.
    EXPECT_NEAR(output.gradient.x, 0, 1e-9);
    EXPECT_NEAR(output.gradient.y, -1, 1e-9);
}

TEST(IrregularBlackBoxOptimizationNoFitPolygon, Hole)
{
    // Fixed item: the square [0, 30] x [0, 30] with the hole
    // [10, 20] x [10, 20]; orbiting item: a 4x4 square. The positions of the
    // orbiting item in the hole, [10, 16] x [10, 16], are a hole of the NFP.
    ShapeWithHoles fixed_shape = rectangle(0, 30, 0, 30);
    fixed_shape.holes.push_back(shape::build_rectangle(10, 20, 10, 20));
    NoFitPolygon no_fit_polygon = build_no_fit_polygon(
            {fixed_shape},
            {rectangle(0, 4, 0, 4)});
    SignedDistance output;

    // In the hole of the NFP: no overlap.
    ASSERT_TRUE(signed_distance(no_fit_polygon, {13, 13}, 5.0, output));
    EXPECT_NEAR(output.distance, -3, 1e-9);
    EXPECT_FALSE(signed_distance(no_fit_polygon, {13, 13}, 2.0, output));

    // Overlapping the left side of the hole: the smallest translation moves
    // the orbiting item into the hole.
    ASSERT_TRUE(signed_distance(no_fit_polygon, {8, 13}, 1.0, output));
    EXPECT_NEAR(output.distance, 2, 1e-9);
    EXPECT_NEAR(output.gradient.x, -1, 1e-9);
    EXPECT_NEAR(output.gradient.y, 0, 1e-9);
}

TEST(IrregularBlackBoxOptimizationNoFitPolygon, SeveralShapes)
{
    // Fixed item made of two 10x10 squares, [0, 10] x [0, 10] and
    // [20, 30] x [0, 10]; orbiting item: a 4x4 square. The positions between
    // the squares, [10, 16] x [-4, 10], don't overlap.
    NoFitPolygon no_fit_polygon = build_no_fit_polygon(
            {rectangle(0, 10, 0, 10), rectangle(20, 30, 0, 10)},
            {rectangle(0, 4, 0, 4)});
    SignedDistance output;
    ASSERT_TRUE(signed_distance(no_fit_polygon, {13, 3}, 5.0, output));
    EXPECT_NEAR(output.distance, -3, 1e-9);
    ASSERT_TRUE(signed_distance(no_fit_polygon, {5, 3}, 1.0, output));
    EXPECT_NEAR(output.distance, 5, 1e-9);
}
