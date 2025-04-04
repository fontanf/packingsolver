#include "packingsolver/irregular/shape.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

struct IrregularShapeElementLengthTestParams
{
    ShapeElement element;
    LengthDbl expected_length;
};

class IrregularShapeElementLengthTest: public testing::TestWithParam<IrregularShapeElementLengthTestParams> { };

TEST_P(IrregularShapeElementLengthTest, IrregularShapeElementLength)
{
    IrregularShapeElementLengthTestParams test_params = GetParam();
    EXPECT_TRUE(equal(test_params.element.length(), test_params.expected_length));
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularShapeElementLengthTest,
        testing::ValuesIn(std::vector<IrregularShapeElementLengthTestParams>{
            {build_shape({{0, 0}, {0, 1}}, true).elements.front(), 1 },
            {build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(), M_PI / 2 },
            {build_shape({{1, 0}, {0, 0, -1}, {0, -1}}, true).elements.front(), M_PI / 2 },
            }));


struct IrregularShapeElementMinMaxTestParams
{
    ShapeElement element;
    LengthDbl expected_x_min;
    LengthDbl expected_y_min;
    LengthDbl expected_x_max;
    LengthDbl expected_y_max;
};

class IrregularShapeElementMinMaxTest: public testing::TestWithParam<IrregularShapeElementMinMaxTestParams> { };

TEST_P(IrregularShapeElementMinMaxTest, IrregularShapeElementMinMax)
{
    IrregularShapeElementMinMaxTestParams test_params = GetParam();
    std::cout << "element " << test_params.element.to_string() << std::endl;
    std::cout << "expected x_min " << test_params.expected_x_min
        << " y_min " << test_params.expected_y_min
        << " x_max " << test_params.expected_x_max
        << " y_max " << test_params.expected_y_max << std::endl;
    auto mm = test_params.element.min_max();
    std::cout << "x_min " << mm.first.x
        << " y_min " << mm.first.y
        << " x_max " << mm.second.x
        << " y_max " << mm.second.y << std::endl;
    EXPECT_TRUE(equal(mm.first.x, test_params.expected_x_min));
    EXPECT_TRUE(equal(mm.second.x, test_params.expected_x_max));
    EXPECT_TRUE(equal(mm.first.y, test_params.expected_y_min));
    EXPECT_TRUE(equal(mm.second.y, test_params.expected_y_max));
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularShapeElementMinMaxTest,
        testing::ValuesIn(std::vector<IrregularShapeElementMinMaxTestParams>{
            {build_shape({{0, 1}, {2, 3}}, true).elements.front(), 0, 1, 2, 3 },
            {build_shape({{1, 0}, {0, 0, 1}, {-1, 0}}, true).elements.front(), -1, 0, 1, 1 },
            {build_shape({{1, 0}, {0, 0, -1}, {-1, 0}}, true).elements.front(), -1, -1, 1, 0 },

            {build_shape({{0, 1}, {0, 0, 1}, {1, 0}}, true).elements.front(), -1, -1, 1, 1 },
            {build_shape({{-1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(), -1, -1, 1, 1 },
            {build_shape({{0, -1}, {0, 0, 1}, {-1, 0}}, true).elements.front(), -1, -1, 1, 1 },
            {build_shape({{1, 0}, {0, 0, 1}, {0, -1}}, true).elements.front(), -1, -1, 1, 1 },
            }));


struct CleanShapeTestParams
{
    Shape shape;
    Shape expected_shape;
};

class IrregularCleanShapeTest: public testing::TestWithParam<CleanShapeTestParams> { };

TEST_P(IrregularCleanShapeTest, CleanShape)
{
    CleanShapeTestParams test_params = GetParam();
    Shape cleaned_shape = clean_shape(test_params.shape);
    std::cout << cleaned_shape.to_string(0) << std::endl;
    EXPECT_EQ(test_params.expected_shape, cleaned_shape);
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularCleanShapeTest,
        testing::ValuesIn(std::vector<CleanShapeTestParams>{
            {
                build_shape({{0, 0}, {0, 0}, {100, 0}, {100, 100}}),
                build_shape({{0, 0}, {100, 0}, {100, 100}})
            }, {
                build_shape({{50, 50}, {0, 0}, {100, 0}, {100, 100}}),
                build_shape({{0, 0}, {100, 0}, {100, 100}})
            }, {
                build_shape({{0, 0}, {100, 0}, {100, 100}, {50, 50}}),
                build_shape({{0, 0}, {100, 0}, {100, 100}})
            }}));


struct IrregularShapeContainsTestParams
{
    Shape shape;
    Point point;
    bool strict;
    bool expected_contained;
};

class IrregularShapeContainsTest: public testing::TestWithParam<IrregularShapeContainsTestParams> { };

TEST_P(IrregularShapeContainsTest, IrregularShapeContains)
{
    IrregularShapeContainsTestParams test_params = GetParam();
    std::cout << "shape" << std::endl;
    std::cout << test_params.shape.to_string(0) << std::endl;
    std::cout << "point " << test_params.point.to_string() << std::endl;
    std::cout << "strict " << test_params.strict << std::endl;
    std::cout << "expected_contained " << test_params.expected_contained << std::endl;

    bool contained = test_params.shape.contains(
            test_params.point,
            test_params.strict);
    std::cout << "contained " << contained << std::endl;

    ASSERT_EQ(contained, test_params.expected_contained);
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularShapeContainsTest,
        testing::ValuesIn(std::vector<IrregularShapeContainsTestParams>{
            {  // Point oustide of polygon
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {2, 2},
                false,
                false,
            }, {  // Point inside polygon
                build_shape({{0, 0}, {2, 0}, {2, 2}, {0, 2}}),
                {1, 1},
                false,
                true,
            }, {  // Point on polygon
                build_shape({{0, 0}, {2, 0}, {2, 2}, {0, 2}}),
                {2, 2},
                false,
                true,
            }, {  // Point on polygon (strict)
                build_shape({{0, 0}, {2, 0}, {2, 2}, {0, 2}}),
                {2, 2},
                true,
                false,
            //}, {  // Point inside circle
            //    build_shape({{0, 2}, {0, 0, 1}, {0, 2}}),
            //    {1, 0},
            //    false,
            //    true,
            }, {  // Point outside of shape
                build_shape({{0, 2}, {0, 0, 1}, {0, -2}}),
                {0, 3},
                false,
                false,
            }, {  // Point inside shape
                build_shape({{0, 2}, {0, 0, 1}, {0, -2}}),
                {0, 1},
                false,
                true,
            }, {  // Point on shape
                build_shape({{0, 2}, {0, 0, 1}, {0, -2}}),
                {0, 2},
                false,
                true,
            }, {  // Point on shape (strict)
                build_shape({{0, 2}, {0, 0, 1}, {0, -2}}),
                {0, 2},
                true,
                false,
            }}));


struct IrregularApproximateCircularArcByLineSegmentsTestParams
{
    ShapeElement circular_arc;
    ElementPos number_of_line_segments;
    bool outer;
    std::vector<ShapeElement> expected_line_segments;
};

class IrregularApproximateCircularArcByLineSegmentsTest: public testing::TestWithParam<IrregularApproximateCircularArcByLineSegmentsTestParams> { };

TEST_P(IrregularApproximateCircularArcByLineSegmentsTest, IrregularApproximateCircularArcByLineSegments)
{
    IrregularApproximateCircularArcByLineSegmentsTestParams test_params = GetParam();
    std::cout << "circular_arc" << std::endl;
    std::cout << test_params.circular_arc.to_string() << std::endl;
    std::cout << "expected_line_segments" << std::endl;
    for (const ShapeElement& line_segment: test_params.expected_line_segments)
        std::cout << line_segment.to_string() << std::endl;
    std::vector<ShapeElement> line_segments = approximate_circular_arc_by_line_segments(
            test_params.circular_arc,
            test_params.number_of_line_segments,
            test_params.outer);
    std::cout << "line_segments" << std::endl;
    for (const ShapeElement& line_segment: line_segments)
        std::cout << line_segment.to_string() << std::endl;

    ASSERT_EQ(line_segments.size(), test_params.number_of_line_segments);
    for (ElementPos pos = 0; pos < test_params.number_of_line_segments; ++pos) {
        //std::cout << std::setprecision (15) << line_segments[pos].start.x << std::endl;
        EXPECT_TRUE(equal(line_segments[pos], test_params.expected_line_segments[pos]));
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularApproximateCircularArcByLineSegmentsTest,
        testing::ValuesIn(std::vector<IrregularApproximateCircularArcByLineSegmentsTestParams>{
            {
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                1,
                false,
                build_shape({{1, 0}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {1, 1, -1}, {0, 1}}, true).elements.front(),
                1,
                true,
                build_shape({{1, 0}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                2,
                true,
                build_shape({{1, 0}, {1, 1}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {1, 1, -1}, {0, 1}}, true).elements.front(),
                2,
                false,
                build_shape({{1, 0}, {0, 0}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                3,
                true,
                build_shape({{1, 0}, {1, 0.414213562373095}, {0.414213562373095, 1}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {1, 1, -1}, {0, 1}}, true).elements.front(),
                3,
                false,
                build_shape({{1, 0}, {0.585786437626905, 0}, {0, 0.585786437626905}, {0, 1}}, true).elements
            }}));
