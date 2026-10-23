#include "irregular/utils.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

struct FindBestEdgeSeparatorTestParams
{
    Shape shape_1;
    Point shift_1;
    double scale_1;
    Shape shape_2;
    Point shift_2;
    double scale_2;

    ShapePos expected_edge_shape_pos;
    ElementPos expected_element_pos;
    Point expected_point;
    LengthDbl expected_distance;
};

inline std::ostream& operator<<(std::ostream& os, const FindBestEdgeSeparatorTestParams&)
{
    return os;
}

class FindBestEdgeSeparatorTest: public testing::TestWithParam<FindBestEdgeSeparatorTestParams> { };

TEST_P(FindBestEdgeSeparatorTest, FindBestEdgeSeparator)
{
    const FindBestEdgeSeparatorTestParams& p = GetParam();
    EdgeSeparationConstraintParameters result = find_best_edge_separator(
            p.shape_1, p.shift_1, p.scale_1,
            p.shape_2, p.shift_2, p.scale_2);

    EXPECT_EQ(result.edge_shape_pos, p.expected_edge_shape_pos);
    EXPECT_EQ(result.edge_element_pos, p.expected_element_pos);
    EXPECT_NEAR(result.point.x, p.expected_point.x, 1e-9);
    EXPECT_NEAR(result.point.y, p.expected_point.y, 1e-9);
    EXPECT_NEAR(result.distance, p.expected_distance, 1e-9);
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        FindBestEdgeSeparatorTest,
        testing::ValuesIn(std::vector<FindBestEdgeSeparatorTestParams>{
            {
                // Two unit squares side by side (gap=1 in x).
                // Best edge: right edge of shape_1 (element 1: (1,0)->(1,1)).
                // Closest vertex of shape_2: (2,0), distance=1.
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {0, 0}, 1.0,
                build_shape({{2, 0}, {3, 0}, {3, 1}, {2, 1}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 0,
                /* expected_element_pos */ 1,
                /* expected_point */ {2, 0},
                /* expected_distance */ 1.0,
            }, {
                // Two unit squares stacked vertically (gap=1 in y).
                // Best edge: top edge of shape_1 (element 2: (1,1)->(0,1)).
                // Closest vertex of shape_2: (0,2), distance=1.
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {0, 0}, 1.0,
                build_shape({{0, 2}, {1, 2}, {1, 3}, {0, 3}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 0,
                /* expected_element_pos */ 2,
                /* expected_point */ {0, 2},
                /* expected_distance */ 1.0,
            }, {
                // Two unit squares touching (gap=0).
                // Best edge: right edge of shape_1 (element 1: (1,0)->(1,1)).
                // Closest vertex of shape_2: (1,0), distance=0.
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {0, 0}, 1.0,
                build_shape({{1, 0}, {2, 0}, {2, 1}, {1, 1}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 0,
                /* expected_element_pos */ 1,
                /* expected_point */ {1, 0},
                /* expected_distance */ 0.0,
            }, {
                // Right triangle shape_1 vs unit square shape_2 placed to the right.
                // shape_1: (0,0)->(4,0)->(0,4), shape_2: (5,0)->(6,0)->(6,1)->(5,1).
                // The left edge of shape_2 (element 3: (5,1)->(5,0)) separates best
                // because shape_1 is entirely to its left with min distance=1
                // (vertex (4,0) of shape_1 is the closest), whereas the hypotenuse
                // of shape_1 gives only min distance=1/sqrt(2) < 1.
                // Expected: edge from shape_2 (edge_shape_pos=1), element 3,
                // closest point (4,0) from shape_1, distance=1.
                build_shape({{0, 0}, {4, 0}, {0, 4}}),
                {0, 0}, 1.0,
                build_shape({{5, 0}, {6, 0}, {6, 1}, {5, 1}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 1,
                /* expected_element_pos */ 3,
                /* expected_point */ {4, 0},
                /* expected_distance */ 1.0,
            }}));
