#include "irregular/shape_supports.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

struct ComputeShapeSupportsTestParams
{
    Shape shape;
    std::vector<Shape> holes;
    std::vector<Shape> expected_supporting_parts;
    std::vector<Shape> expected_supported_parts;
};

class IrregularComputeShapeSupportsTest: public testing::TestWithParam<ComputeShapeSupportsTestParams> { };

TEST_P(IrregularComputeShapeSupportsTest, ComputeShapeSupports)
{
    ComputeShapeSupportsTestParams test_params = GetParam();
    std::cout << "shape:" << std::endl;
    std::cout << "- " << test_params.shape.to_string(0) << std::endl;
    std::cout << "holes:" << std::endl;
    for (const Shape& hole: test_params.holes)
        std::cout << "- " << hole.to_string(2) << std::endl;

    ShapeSupports supports = compute_shape_supports(test_params.shape, test_params.holes);
    std::cout << "supporting_parts:" << std::endl;
    for (const auto& support: supports.supporting_parts) {
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)support.elements.size();
                ++element_pos) {
            const ShapeElement& element = support.elements[element_pos];
            if (element_pos == 0) {
                std::cout << "- " << element.to_string() << std::endl;
            } else {
                std::cout << "  " << element.to_string() << std::endl;
            }
        }
    }
    std::cout << "supported_parts:" << std::endl;
    for (const auto& support: supports.supported_parts) {
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)support.elements.size();
                ++element_pos) {
            const ShapeElement& element = support.elements[element_pos];
            if (element_pos == 0) {
                std::cout << "- " << element.to_string() << std::endl;
            } else {
                std::cout << "  " << element.to_string() << std::endl;
            }
        }
    }

    ASSERT_EQ(supports.supporting_parts.size(), test_params.expected_supporting_parts.size());
    ASSERT_EQ(supports.supported_parts.size(), test_params.expected_supported_parts.size());
    for (ElementPos pos = 0;
            pos < (ElementPos)supports.supporting_parts.size();
            ++pos) {
        Shape support = supports.supporting_parts[pos];
        Shape expected_support = test_params.expected_supporting_parts[pos];
        ASSERT_EQ(supports.supported_parts.size(), test_params.expected_supported_parts.size());
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)support.elements.size();
                ++element_pos) {
            EXPECT_EQ(support.elements[element_pos], expected_support.elements[element_pos]);
        }
    }
    for (ElementPos pos = 0;
            pos < (ElementPos)supports.supported_parts.size();
            ++pos) {
        Shape support = supports.supported_parts[pos];
        Shape expected_support = test_params.expected_supported_parts[pos];
        ASSERT_EQ(supports.supported_parts.size(), test_params.expected_supported_parts.size());
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)support.elements.size();
                ++element_pos) {
            EXPECT_EQ(support.elements[element_pos], expected_support.elements[element_pos]);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularComputeShapeSupportsTest,
        testing::ValuesIn(std::vector<ComputeShapeSupportsTestParams>{
            {
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {},
                {build_shape({{0, 1}, {1, 1}}, true)},
                {build_shape({{0, 0}, {1, 0}}, true)},
            }, {
                build_shape({{0, 0}, {1, 0}, {2, 1}, {1, 1}}),
                {},
                {build_shape({{0, 0}, {1, 1}, {2, 1}}, true)},
                {build_shape({{0, 0}, {1, 0}, {2, 1}}, true)},
            }, {
                build_shape({{0, 0}, {7, 0}, {6, 2}, {5, 2}, {4, 1}, {3, 1}, {2, 2}, {1, 2}}),
                {},
                {build_shape({{0, 0}, {1, 2}, {2, 2}, {3, 1}, {4, 1}, {5, 2}, {6, 2}, {7, 0}}, true)},
                {build_shape({{0, 0}, {7, 0}}, true)},
            }, {
                build_shape({{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {2, 2}, {2, 3}, {0, 3}}),
                {},
                {
                    build_shape({{1, 1}, {2, 1}}, true),
                    build_shape({{0, 3}, {2, 3}}, true),
                }, {
                    build_shape({{0, 0}, {2, 0}}, true),
                    build_shape({{1, 2}, {2, 2}}, true),
                },
            }, {
                build_shape({{0, 0}, {3, 0}, {3, 3}, {0, 3}}),
                {build_shape({{1, 1}, {2, 1}, {2, 2}, {1, 2}}),},
                {
                    build_shape({{0, 3}, {3, 3}}, true),
                    build_shape({{1, 1}, {2, 1}}, true),
                }, {
                    build_shape({{0, 0}, {3, 0}}, true),
                    build_shape({{1, 2}, {2, 2}}, true),
                },
            }}));
