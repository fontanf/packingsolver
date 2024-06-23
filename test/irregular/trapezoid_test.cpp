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
