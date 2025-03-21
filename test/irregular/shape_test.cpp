#include "packingsolver/irregular/shape.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

TEST(IrregularShape, CleanShapeRedundant)
{
    Shape shape = build_shape({{0, 0}, {0, 0}, {100, 0}, {100, 100}});

    Shape cleaned_shape = clean_shape(shape);
    std::cout << cleaned_shape.to_string(0) << std::endl;

    Shape expected_shape = build_shape({{0, 0}, {100, 0}, {100, 100}});
    EXPECT_EQ(expected_shape, cleaned_shape);
}

TEST(IrregularShape, CleanShapeAligned)
{
    Shape shape = build_shape({{50, 50}, {0, 0}, {100, 0}, {100, 100}});

    Shape cleaned_shape = clean_shape(shape);

    Shape expected_shape = build_shape({{0, 0}, {100, 0}, {100, 100}});
    EXPECT_EQ(expected_shape, cleaned_shape);
}

TEST(IrregularShape, CleanShapeAligned2)
{
    Shape shape = build_shape({{0, 0}, {100, 0}, {100, 100}, {50, 50}});

    Shape cleaned_shape = clean_shape(shape);

    Shape expected_shape = build_shape({{0, 0}, {100, 0}, {100, 100}});
    EXPECT_EQ(expected_shape, cleaned_shape);
}

TEST(IrregularShape, Borders0)
{
    Shape shape = build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}});
    std::vector<Shape> expected_borders = {};

    std::vector<Shape> shape_borders = borders(shape);
    for (const Shape& border: shape_borders)
        std::cout << border.to_string(0) << std::endl;

    EXPECT_EQ(shape_borders.size(), expected_borders.size());
    for (const Shape& expected_border: expected_borders) {
        EXPECT_NE(std::find(
                    shape_borders.begin(),
                    shape_borders.end(),
                    expected_border),
                shape_borders.end());
    }
}

TEST(IrregularShape, Borders1)
{
    Shape shape = build_shape({{2, 0}, {3, 1}, {0, 1}});
    std::vector<Shape> expected_borders = {
        build_shape({{3, 0}, {3, 1}, {2, 0}}),
        build_shape({{0, 0}, {2, 0}, {0, 1}}),
    };

    std::vector<Shape> shape_borders = borders(shape);
    for (const Shape& border: shape_borders)
        std::cout << border.to_string(0) << std::endl;

    EXPECT_EQ(shape_borders.size(), expected_borders.size());
    for (const Shape& expected_border: expected_borders) {
        EXPECT_NE(std::find(
                    shape_borders.begin(),
                    shape_borders.end(),
                    expected_border),
                shape_borders.end());
    }
}

TEST(IrregularShape, Borders2)
{
    Shape shape = build_shape({{0, 0}, {3, 1}, {0, 1}});
    std::vector<Shape> expected_borders = {
        build_shape({{3, 0}, {3, 1}, {0, 0}}),
    };

    std::vector<Shape> shape_borders = borders(shape);
    for (const Shape& border: shape_borders)
        std::cout << border.to_string(0) << std::endl;

    EXPECT_EQ(shape_borders.size(), expected_borders.size());
    for (const Shape& expected_border: expected_borders) {
        EXPECT_NE(std::find(
                    shape_borders.begin(),
                    shape_borders.end(),
                    expected_border),
                shape_borders.end());
    }
}

TEST(IrregularShape, Borders3)
{
    Shape shape = build_shape({{0, 0}, {50, 0}, {30, 30}});
    std::vector<Shape> expected_borders = {
        build_shape({{0, 0}, {30, 30}, {0, 30}}),
        build_shape({{30, 30}, {50, 0}, {50, 30}}),
    };

    std::vector<Shape> shape_borders = borders(shape);
    for (const Shape& border: shape_borders)
        std::cout << border.to_string(0) << std::endl;

    EXPECT_EQ(shape_borders.size(), expected_borders.size());
    for (const Shape& expected_border: expected_borders) {
        EXPECT_NE(std::find(
                    shape_borders.begin(),
                    shape_borders.end(),
                    expected_border),
                shape_borders.end());
    }
}
