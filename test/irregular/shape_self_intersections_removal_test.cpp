#include "irregular/shape_self_intersections_removal.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;


struct IrregularSelfIntersectionsRemovalTestParams
{
    Shape shape;
    Shape expected_shape;
    std::vector<Shape> expected_holes;
};

class IrregularSelfIntersectionsRemovalTest: public testing::TestWithParam<IrregularSelfIntersectionsRemovalTestParams> { };

TEST_P(IrregularSelfIntersectionsRemovalTest, IrregularSelfIntersectionsRemoval)
{
    IrregularSelfIntersectionsRemovalTestParams test_params = GetParam();
    std::cout << "shape " << test_params.shape.to_string(0) << std::endl;
    std::cout << "expected_shape " << test_params.expected_shape.to_string(0) << std::endl;
    std::cout << "expected_holes" << std::endl;
    for (const Shape& hole: test_params.expected_holes)
        std::cout << "- " << hole.to_string(2) << std::endl;

    auto p = remove_self_intersections(test_params.shape);
    std::cout << "shape " << p.first.to_string(0) << std::endl;
    std::cout << "holes" << std::endl;
    for (const Shape& hole: p.second)
        std::cout << "- " << hole.to_string(2) << std::endl;

    EXPECT_TRUE(equal(p.first, test_params.expected_shape));
    ASSERT_EQ(p.second.size(), test_params.expected_holes.size());
    for (const Shape& expected_hole: test_params.expected_holes) {
        EXPECT_NE(std::find(
                    p.second.begin(),
                    p.second.end(),
                    expected_hole),
                p.second.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularSelfIntersectionsRemovalTest,
        testing::ValuesIn(std::vector<IrregularSelfIntersectionsRemovalTestParams>{
            {  // No self intersection
                build_shape({{0, 0}, {4, 0}, {4, 4}, {2.5, 4}, {2.5, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {1.5, 3}, {1.5, 4}, {0, 4}}),
                build_shape({{0, 0}, {4, 0}, {4, 4}, {2.5, 4}, {2.5, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {1.5, 3}, {1.5, 4}, {0, 4}}),
                {},
            }, {
                build_shape({{0, 0}, {4, 0}, {4, 3}, {3, 3}, {1, 4}, {3, 1}, {1, 1}, {3, 4}, {1, 3}, {0, 3}}),
                build_shape({{0, 0}, {4, 0}, {4, 3}, {3, 3}, {2.5, 3.25}, {3, 4}, {2, 3.5}, {1, 4}, {1.5, 3.25}, {1, 3}, {0, 3}}),
                {build_shape({{1, 1}, {3, 1}, {2, 2.5}})},
            }, {
                build_shape({{0, 0}, {4, 0}, {4, 4}, {1, 4}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {3, 4}, {0, 4}}),
                build_shape({{0, 0}, {4, 0}, {4, 4}, {0, 4}}),
                {build_shape({{1, 1}, {3, 1}, {3, 3}, {2, 3.5}, {1, 3}})},
            }, {
                build_shape({{0, 0}, {4, 0}, {4, 4}, {2, 4}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {2, 4}, {0, 4}}),
                build_shape({{0, 0}, {4, 0}, {4, 4}, {0, 4} }),
                {build_shape({ {1, 1}, {3, 1}, {3, 3}, {2, 4}, {1, 3}})},
            }, {
                build_shape({{0, 0}, {4, 0}, {4, 4}, {2, 4}, {3, 3}, {3, 1}, {2, 4}, {1, 1}, {1, 3}, {2, 4}, {0, 4}}),
                build_shape({{0, 0}, {4, 0}, {4, 4}, {0, 4}}),
                {
                    build_shape({{3, 1}, {3, 3}, {2, 4}}),
                    build_shape({{1, 1}, {2, 4}, {1, 3}})
                },
            }, {
                build_shape({{0, 0}, {4, 0}, {4, 4}, {2, 4}, {2, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {2, 3}, {2, 4}, {0, 4}}),
                build_shape({{0, 0}, {4, 0}, {4, 4}, {0, 4}}),
                {build_shape({{1, 1}, {3, 1}, {3, 3}, {1, 3}})}
            }, {
                build_shape({{0, 0}, {4, 0}, {4, 4}, {1, 4}, {1, 1}, {3, 1}, {3, 4}, {0, 4}}),
                build_shape({{0, 0}, {4, 0}, {4, 4}, {0, 4}}),
                {}
            }}));


struct IrregularExtractHolesTestParams
{
    Shape hole;
    std::vector<Shape> expected_holes;
};

class IrregularExtractHolesTest: public testing::TestWithParam<IrregularExtractHolesTestParams> { };

TEST_P(IrregularExtractHolesTest, IrregularExtractHoles)
{
    IrregularExtractHolesTestParams test_params = GetParam();
    std::cout << "hole " << test_params.hole.to_string(0) << std::endl;
    std::cout << "expected_holes" << std::endl;
    for (const Shape& hole: test_params.expected_holes)
        std::cout << "- " << hole.to_string(2) << std::endl;

    auto holes = extract_all_holes_from_self_intersecting_hole(test_params.hole);
    std::cout << "holes" << std::endl;
    for (const Shape& hole: holes)
        std::cout << "- " << hole.to_string(2) << std::endl;

    ASSERT_EQ(holes.size(), test_params.expected_holes.size());
    for (const Shape& expected_hole: test_params.expected_holes) {
        EXPECT_NE(std::find(
                    holes.begin(),
                    holes.end(),
                    expected_hole),
                holes.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularExtractHolesTest,
        testing::ValuesIn(std::vector<IrregularExtractHolesTestParams>{
            {  // No self intersection
                build_shape({{0, 0}, {4, 0}, {4, 4}, {2.5, 4}, {2.5, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {1.5, 3}, {1.5, 4}, {0, 4}}),
                {build_shape({{0, 0}, {4, 0}, {4, 4}, {2.5, 4}, {2.5, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {1.5, 3}, {1.5, 4}, {0, 4}})},
            }, {
                build_shape({{0, 0}, {4, 0}, {0, 2}, {4, 4}, {0, 4}, {4, 2}}),
                {
                    build_shape({{0, 0}, {4, 0}, {2, 1}}),
                    build_shape({{2, 3}, {4, 4}, {0, 4}}),
                },
            }}));


struct IrregularComputeUnionTestParams
{
    Shape shape_1;
    Shape shape_2;
    Shape expected_shape;
    std::vector<Shape> expected_holes;
};

class IrregularComputeUnionTest: public testing::TestWithParam<IrregularComputeUnionTestParams> { };

TEST_P(IrregularComputeUnionTest, IrregularComputeUnion)
{
    IrregularComputeUnionTestParams test_params = GetParam();
    std::cout << "shape_1 " << test_params.shape_1.to_string(0) << std::endl;
    std::cout << "shape_2 " << test_params.shape_2.to_string(0) << std::endl;
    std::cout << "expected_shape " << test_params.expected_shape.to_string(0) << std::endl;
    std::cout << "expected_holes" << std::endl;
    for (const Shape& hole: test_params.expected_holes)
        std::cout << "- " << hole.to_string(2) << std::endl;

    auto p = compute_union(
            test_params.shape_1,
            test_params.shape_2);
    std::cout << "shape " << p.first.to_string(0) << std::endl;
    std::cout << "holes" << std::endl;
    for (const Shape& hole: p.second)
        std::cout << "- " << hole.to_string(2) << std::endl;

    EXPECT_TRUE(equal(p.first, test_params.expected_shape));
    ASSERT_EQ(p.second.size(), test_params.expected_holes.size());
    for (const Shape& expected_hole: test_params.expected_holes) {
        EXPECT_NE(std::find(
                    p.second.begin(),
                    p.second.end(),
                    expected_hole),
                p.second.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularComputeUnionTest,
        testing::ValuesIn(std::vector<IrregularComputeUnionTestParams>{
            {
                build_shape({{0, 0}, {2, 0}, {2, 1}, {0, 1}}),
                build_shape({{1, 0}, {3, 0}, {3, 1}, {1, 1}}),
                build_shape({{0, 0}, {3, 0}, {3, 1}, {0, 1}}),
                {},
            }, {
                build_shape({{0, 0}, {2, 0}, {2, 2}, {0, 2}}),
                build_shape({{1, 1}, {3, 1}, {3, 3}, {1, 3}}),
                build_shape({{0, 0}, {2, 0}, {2, 1}, {3, 1}, {3, 3}, {1, 3}, {1, 2}, {0, 2}}),
                {},
            }, {
                build_shape({{0, 0}, {3, 0}, {3, 1}, {1, 1}, {1, 2}, {3, 2}, {3, 3}, {0, 3}}),
                build_shape({{2, 0}, {5, 0}, {5, 3}, {2, 3}, {2, 2}, {4, 2}, {4, 1}, {2, 1}}),
                build_shape({{0, 0}, {5, 0}, {5, 3}, {0, 3}}),
                {build_shape({{1, 1}, {4, 1}, {4, 2}, {1, 2}})},
            }}));
