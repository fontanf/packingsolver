#include "irregular/polygon_convex_hull.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

TEST(IrregularPolygonConvexHull, Triangle)
{
    Shape shape = build_polygon_shape({{0, 0}, {3, 0}, {1, 3}});
    Shape convex_hull = polygon_convex_hull(shape);

    ASSERT_EQ(convex_hull.elements.size(), 3);
    EXPECT_EQ(convex_hull.elements[0].start.x, 0);
    EXPECT_EQ(convex_hull.elements[0].start.y, 0);
    EXPECT_EQ(convex_hull.elements[1].start.x, 3);
    EXPECT_EQ(convex_hull.elements[1].start.y, 0);
    EXPECT_EQ(convex_hull.elements[2].start.x, 1);
    EXPECT_EQ(convex_hull.elements[2].start.y, 3);
}
