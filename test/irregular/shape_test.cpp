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
