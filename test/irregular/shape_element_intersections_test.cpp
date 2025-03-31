#include "irregular/shape_element_intersections.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;


struct IrregularComputeIntersectionsTestParams
{
    ShapeElement element_1;
    ShapeElement element_2;
    std::vector<Point> expected_intersections;
};

class IrregularComputeIntersectionsTest: public testing::TestWithParam<IrregularComputeIntersectionsTestParams> { };

TEST_P(IrregularComputeIntersectionsTest, IrregularComputeIntersections)
{
    IrregularComputeIntersectionsTestParams test_params = GetParam();
    std::cout << "element_1 " << test_params.element_1.to_string() << std::endl;
    std::cout << "element_2 " << test_params.element_2.to_string() << std::endl;
    std::cout << "expected_intersections" << std::endl;
    for (const Point& point: test_params.expected_intersections)
        std::cout << "- " << point.to_string() << std::endl;

    std::vector<Point> intersections = compute_intersections(
            test_params.element_1,
            test_params.element_2);
    std::cout << "intersections" << std::endl;
    for (const Point& point: intersections)
        std::cout << "- " << point.to_string() << std::endl;

    ASSERT_EQ(intersections.size(), test_params.expected_intersections.size());
    for (const Point& expected_intersection: test_params.expected_intersections) {
        EXPECT_NE(std::find(
                    intersections.begin(),
                    intersections.end(),
                    expected_intersection),
                intersections.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularComputeIntersectionsTest,
        testing::ValuesIn(std::vector<IrregularComputeIntersectionsTestParams>{
            {  // Non-intersecting line segments
                build_shape({{0, 0}, {0, 1}}, true).elements.front(),
                build_shape({{1, 0}, {1, 1}}, true).elements.front(),
                {},
            }, {  // Simple line segment intersection.
                build_shape({{1, 0}, {1, 2}}, true).elements.front(),
                build_shape({{0, 1}, {2, 1}}, true).elements.front(),
                {{1, 1}},
            }, {  // Non-intersecting line segment and circular arc.
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                build_shape({{2, 0}, {2, 2}}, true).elements.front(),
                {},
            }, {  // Non-intersecting circular arcs.
                build_shape({{2, 0}, {0, 0, 1}, {0, 2}}, true).elements.front(),
                build_shape({{3, 0}, {1, 0, 1}, {1, 2}}, true).elements.front(),
                {},
            }}));
