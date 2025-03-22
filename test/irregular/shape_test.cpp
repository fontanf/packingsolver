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


struct ExtractBordersTestParams
{
    Shape shape;
    std::vector<Shape> expected_borders;
};

class IrregularExtractBordersTest: public testing::TestWithParam<ExtractBordersTestParams> { };

TEST_P(IrregularExtractBordersTest, ExtractBorders)
{
    ExtractBordersTestParams test_params = GetParam();
    std::vector<Shape> shape_borders = borders(test_params.shape);
    for (const Shape& border: shape_borders)
        std::cout << border.to_string(0) << std::endl;
    EXPECT_EQ(shape_borders.size(), test_params.expected_borders.size());
    for (const Shape& expected_border: test_params.expected_borders) {
        EXPECT_NE(std::find(
                    shape_borders.begin(),
                    shape_borders.end(),
                    expected_border),
                shape_borders.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularExtractBordersTest,
        testing::ValuesIn(std::vector<ExtractBordersTestParams>{
            {
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {},
            }, {
                build_shape({{2, 0}, {3, 1}, {0, 1}}),
                {
                    build_shape({{3, 0}, {3, 1}, {2, 0}}),
                    build_shape({{0, 0}, {2, 0}, {0, 1}}),
                }
            }, {
                build_shape({{0, 0}, {3, 1}, {0, 1}}),
                {
                    build_shape({{3, 0}, {3, 1}, {0, 0}}),
                }
            }, {
                build_shape({{0, 0}, {50, 0}, {30, 30}}),
                {
                    build_shape({{0, 0}, {30, 30}, {0, 30}}),
                    build_shape({{30, 30}, {50, 0}, {50, 30}}),
                }
            }}));
