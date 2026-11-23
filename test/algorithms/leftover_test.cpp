#include "algorithms/leftover.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;

TEST(Leftover, Rectangle)
{
    // Value of a w x h rectangle, h <= w: h^2 (3w - h) / 12.
    EXPECT_NEAR(box_leftover_value<2>({1500, 1000}), 1000.0 * 1000 * (3 * 1500 - 1000) / 12, 1e-3);
    EXPECT_NEAR(box_leftover_value<2>({1000, 1500}), 1000.0 * 1000 * (3 * 1500 - 1000) / 12, 1e-3);
    EXPECT_NEAR(box_leftover_value<2>({1200, 1200}), 1200.0 * 1200 * 1200 / 6, 1e-3);
    // A used box spanning the bin along y leaves a rectangle.
    EXPECT_NEAR(
            leftover_value<2>({3000, 1500}, {1000, 1500}),
            box_leftover_value<2>({2000, 1500}),
            1e-3);
}

TEST(Leftover, Segment)
{
    // In one dimension, the value of a segment of length l is l^2 / 4.
    EXPECT_NEAR(leftover_value<1>({100}, {40}), 60.0 * 60 / 4, 1e-6);
}

TEST(Leftover, Cube)
{
    // Value of a cube of side s: integral of (s - 2t)^3 from 0 to s / 2, that
    // is s^4 / 8.
    EXPECT_NEAR(box_leftover_value<3>({100, 100, 100}), 100.0 * 100 * 100 * 100 / 8, 1e-3);
}

TEST(Leftover, LShape)
{
    // Bin 3000 x 1500 minus 2000 x 1000: the leftover shrunk by t has area
    // (3000 - 2t)(1500 - 2t) - 2000 * 1000 for t <= 250, then
    // (3000 - 2t)(1500 - 2t) - 2000 (1500 - 2t) = (1000 - 2t)(1500 - 2t) for
    // t <= 500, then 0.
    double expected
        = (3000.0 * 1500 - 2000.0 * 1000) * 250
        - 2 * (3000 + 1500) * 250.0 * 250 / 2
        + 4 * 250.0 * 250 * 250 / 3;
    // Integral of (1000 - 2t)(1500 - 2t) from 250 to 500.
    auto f = [](double t) { return 1000.0 * 1500 * t - (1000 + 1500) * t * t + 4 * t * t * t / 3; };
    expected += f(500) - f(250);
    EXPECT_NEAR(leftover_value<2>({3000, 1500}, {2000, 1000}), expected, 1e-9 * expected);
    // Worth more than any cut into two rectangles.
    EXPECT_GT(
            leftover_value<2>({3000, 1500}, {2000, 1000}),
            cut_leftover_value<2>({3000, 1500}, {2000, 1000}));
}

TEST(Leftover, Cut)
{
    // Bin 3000 x 1500 minus 2000 x 1000: cut vertically into 1000 x 1500 and
    // 2000 x 500, or horizontally into 3000 x 500 and 1000 x 1000.
    double vertical = box_leftover_value<2>({1000, 1500}) + box_leftover_value<2>({2000, 500});
    double horizontal = box_leftover_value<2>({3000, 500}) + box_leftover_value<2>({1000, 1000});
    EXPECT_NEAR(
            cut_leftover_value<2>({3000, 1500}, {2000, 1000}),
            std::max(vertical, horizontal),
            1e-3);
    // Without a used box, the leftover is the whole bin.
    EXPECT_NEAR(
            cut_leftover_value<2>({3000, 1500}, {0, 0}),
            box_leftover_value<2>({3000, 1500}),
            1e-3);
    // With a used box spanning the bin, there is no leftover.
    EXPECT_NEAR(cut_leftover_value<2>({3000, 1500}, {3000, 1500}), 0.0, 1e-9);
    EXPECT_NEAR(leftover_value<2>({3000, 1500}, {3000, 1500}), 0.0, 1e-9);
}

TEST(Leftover, Monotone)
{
    // A larger used box gives a smaller value.
    EXPECT_GT(
            leftover_value<3>({1000, 800, 600}, {500, 400, 300}),
            leftover_value<3>({1000, 800, 600}, {600, 400, 300}));
    EXPECT_GT(
            leftover_value<2>({1000, 800}, {500, 400}),
            leftover_value<2>({1000, 800}, {500, 401}));
}

TEST(Leftover, Concentrated)
{
    // Two bins 3000 x 1500 with 3 m^2 free in total: one leftover of
    // 1500 x 1500 and one of 500 x 1500 is worth more than two leftovers of
    // 1000 x 1500.
    EXPECT_GT(
            leftover_value<2>({3000, 1500}, {1500, 1500})
            + leftover_value<2>({3000, 1500}, {2500, 1500}),
            2 * leftover_value<2>({3000, 1500}, {2000, 1500}));
}

TEST(Leftover, StaircaseSingleStep)
{
    // A staircase with a single step is an L-shape.
    for (LeftoverValueType leftover_value_type: {
            LeftoverValueType::SquaredArea,
            LeftoverValueType::Depth}) {
        double expected = leftover_value<2>(leftover_value_type, {3000, 1500}, {2000, 1000}, false);
        EXPECT_NEAR(
                staircase_leftover_value<2>(leftover_value_type, {3000, 1500}, {{2000, 1000}}),
                expected,
                1e-9 * expected);
        expected = leftover_value<3>(leftover_value_type, {100, 50, 40}, {60, 20, 10}, false);
        EXPECT_NEAR(
                staircase_leftover_value<3>(leftover_value_type, {100, 50, 40}, {{60, 20, 10}}),
                expected,
                1e-9 * expected);
    }
}

TEST(Leftover, StaircaseEmpty)
{
    // Without steps, the leftover is the whole bin.
    EXPECT_NEAR(
            staircase_leftover_value<2>(LeftoverValueType::Depth, {1500, 1000}, {}),
            box_leftover_value<2>({1500, 1000}),
            1e-3);
}

TEST(Leftover, StaircaseSquaredArea)
{
    // Bin 12 x 10 minus the steps 6 x 4 and 3 x 8: area 120 - 24 - 12 = 84.
    EXPECT_NEAR(
            staircase_leftover_value<2>(LeftoverValueType::SquaredArea, {12, 10}, {{6, 4}, {3, 8}}),
            84.0 * 84.0,
            1e-9);
    // Bin 10 x 10 x 10 minus the boxes 5 x 5 x 10 and 10 x 5 x 5: volume
    // 1000 - 250 - 250 + 125 = 625.
    EXPECT_NEAR(
            staircase_leftover_value<3>(LeftoverValueType::SquaredArea, {10, 10, 10}, {{5, 5, 10}, {10, 5, 5}}),
            625.0 * 625.0,
            1e-9);
}

TEST(Leftover, StaircaseDepth)
{
    // Bin 12 x 10 minus the steps 6 x 4 and 3 x 8, against a numerical
    // integration of the distance to the edge on a fine grid. The distance
    // of a point to the edge is its distance to the border of the bin or to
    // a step, whichever is smaller; the distance to the step [0, ex] x
    // [0, ey] of a point outside it is max(x - ex, y - ey).
    double w = 12;
    double h = 10;
    std::vector<std::array<double, 2>> steps = {{6, 4}, {3, 8}};
    int n = 1200;
    double cell_x = w / n;
    double cell_y = h / n;
    double expected = 0;
    for (int ix = 0; ix < n; ++ix) {
        for (int iy = 0; iy < n; ++iy) {
            double x = (ix + 0.5) * cell_x;
            double y = (iy + 0.5) * cell_y;
            double distance = std::min(std::min(x, w - x), std::min(y, h - y));
            bool inside_step = false;
            for (const auto& step: steps) {
                if (x < step[0] && y < step[1])
                    inside_step = true;
                distance = std::min(distance, std::max(x - step[0], y - step[1]));
            }
            if (!inside_step)
                expected += distance * cell_x * cell_y;
        }
    }
    double value = staircase_leftover_value<2>(LeftoverValueType::Depth, {w, h}, steps);
    EXPECT_NEAR(value, expected, 1e-3 * expected);
    // The staircase is worth more than the L-shape of its bounding box.
    EXPECT_GT(value, leftover_value<2>({w, h}, {6, 8}));
}
