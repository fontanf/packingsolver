#include "irregular/trapezoid.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

TEST(IrregularTrapezoid, Intersection1)
{
    GeneralizedTrapezoid trapezoid_1(-25, 175, 375, 775, 575, 975);
    GeneralizedTrapezoid trapezoid_2(0, 200, 0, 400, 200, 600);
    EXPECT_EQ(trapezoid_1.compute_right_shift(trapezoid_2), 0);
    EXPECT_EQ(trapezoid_1.compute_right_shift_if_intersects(trapezoid_2), 0);
}

TEST(IrregularTrapezoid, Intersection2)
{
    GeneralizedTrapezoid trapezoid_1(-25, 175, 350, 750, 550, 950);
    GeneralizedTrapezoid trapezoid_2(0, 200, 0, 400, 200, 600);
    EXPECT_EQ(trapezoid_1.compute_right_shift(trapezoid_2), 25);
    EXPECT_EQ(trapezoid_1.compute_right_shift_if_intersects(trapezoid_2), 25);
}

TEST(IrregularTrapezoid, Intersection3)
{
    GeneralizedTrapezoid trapezoid_1(-25, 175, -425, -25, -225, 175);
    GeneralizedTrapezoid trapezoid_2(0, 200, 0, 400, 200, 600);
    EXPECT_EQ(trapezoid_1.compute_right_shift(trapezoid_2), 800);
    EXPECT_EQ(trapezoid_1.compute_right_shift_if_intersects(trapezoid_2), 0);
}

TEST(IrregularTrapezoid, TopRightShift1)
{
    GeneralizedTrapezoid trapezoid_1(0, 2, 0, 4, 1, 3);
    GeneralizedTrapezoid trapezoid_2(1, 3, 2, 4, 1, 5);
    auto p = trapezoid_1.compute_top_right_shift(trapezoid_2, 1);
    EXPECT_EQ(p.first, 3);
    EXPECT_EQ(p.second, 3);
}

TEST(IrregularTrapezoid, TopRightShift2)
{
    GeneralizedTrapezoid trapezoid_1(0, 2, 1.5, 5.5, 2.5, 4.5);
    GeneralizedTrapezoid trapezoid_2(1, 3, 1, 3, 0, 4);
    auto p = trapezoid_1.compute_top_right_shift(trapezoid_2, 1);
    EXPECT_EQ(p.first, 2);
    EXPECT_EQ(p.second, 2);
}
