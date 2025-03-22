#include "packingsolver/irregular/shape.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

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


struct ApproximateCircularArcByLineSegmentsTestParams
{
    ShapeElement circular_arc;
    ElementPos number_of_line_segments;
    bool outer;
    std::vector<ShapeElement> expected_line_segments;
};

class IrregularApproximateCircularArcByLineSegmentsTest: public testing::TestWithParam<ApproximateCircularArcByLineSegmentsTestParams> { };

TEST_P(IrregularApproximateCircularArcByLineSegmentsTest, ApproximateCircularArcByLineSegments)
{
    ApproximateCircularArcByLineSegmentsTestParams test_params = GetParam();
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
        EXPECT_TRUE(near(line_segments[pos], test_params.expected_line_segments[pos]));
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularApproximateCircularArcByLineSegmentsTest,
        testing::ValuesIn(std::vector<ApproximateCircularArcByLineSegmentsTestParams>{
            {
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                1,
                false,
                build_shape({{1, 0}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                2,
                true,
                build_shape({{1, 0}, {1, 1}, {0, 1}}, true).elements
            }, {
                build_shape({{1, 0}, {0, 0, 1}, {0, 1}}, true).elements.front(),
                3,
                true,
                build_shape({{1, 0}, {1, 0.414213562373095}, {0.414213562373095, 1}, {0, 1}}, true).elements
            }}));
