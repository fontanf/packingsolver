#include "irregular/shape.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

TEST(IrregularShape, CleanShapeRedundant)
{
    Shape shape;

    ShapeElement element_1;
    element_1.type = ShapeElementType::LineSegment;
    element_1.start = {0, 0};
    element_1.end = {0, 0};
    shape.elements.push_back(element_1);

    ShapeElement element_2;
    element_2.type = ShapeElementType::LineSegment;
    element_2.start = {0, 0};
    element_2.end = {100, 0};
    shape.elements.push_back(element_2);

    ShapeElement element_3;
    element_3.type = ShapeElementType::LineSegment;
    element_3.start = {100, 0};
    element_3.end = {100, 100};
    shape.elements.push_back(element_3);

    ShapeElement element_4;
    element_4.type = ShapeElementType::LineSegment;
    element_4.start = {100, 100};
    element_4.end = {0, 0};
    shape.elements.push_back(element_4);

    Shape cleaned_shape = clean_shape(shape);

    EXPECT_EQ(cleaned_shape.elements.size(), 3);
    EXPECT_EQ(cleaned_shape.elements[0].start.x, 0);
    EXPECT_EQ(cleaned_shape.elements[0].start.y, 0);
    EXPECT_EQ(cleaned_shape.elements[0].end.x, 100);
    EXPECT_EQ(cleaned_shape.elements[0].end.y, 0);
    EXPECT_EQ(cleaned_shape.elements[1].start.x, 100);
    EXPECT_EQ(cleaned_shape.elements[1].start.y, 0);
    EXPECT_EQ(cleaned_shape.elements[1].end.x, 100);
    EXPECT_EQ(cleaned_shape.elements[1].end.y, 100);
    EXPECT_EQ(cleaned_shape.elements[2].start.x, 100);
    EXPECT_EQ(cleaned_shape.elements[2].start.y, 100);
    EXPECT_EQ(cleaned_shape.elements[2].end.x, 0);
    EXPECT_EQ(cleaned_shape.elements[2].end.y, 0);
}

TEST(IrregularShape, CleanShapeAligned)
{
    Shape shape;

    ShapeElement element_1;
    element_1.type = ShapeElementType::LineSegment;
    element_1.start = {50, 50};
    element_1.end = {0, 0};
    shape.elements.push_back(element_1);

    ShapeElement element_2;
    element_2.type = ShapeElementType::LineSegment;
    element_2.start = {0, 0};
    element_2.end = {100, 0};
    shape.elements.push_back(element_2);

    ShapeElement element_3;
    element_3.type = ShapeElementType::LineSegment;
    element_3.start = {100, 0};
    element_3.end = {100, 100};
    shape.elements.push_back(element_3);

    ShapeElement element_4;
    element_4.type = ShapeElementType::LineSegment;
    element_4.start = {100, 100};
    element_4.end = {50, 50};
    shape.elements.push_back(element_4);

    Shape cleaned_shape = clean_shape(shape);

    EXPECT_EQ(cleaned_shape.elements.size(), 3);
    EXPECT_EQ(cleaned_shape.elements[0].start.x, 0);
    EXPECT_EQ(cleaned_shape.elements[0].start.y, 0);
    EXPECT_EQ(cleaned_shape.elements[0].end.x, 100);
    EXPECT_EQ(cleaned_shape.elements[0].end.y, 0);
    EXPECT_EQ(cleaned_shape.elements[1].start.x, 100);
    EXPECT_EQ(cleaned_shape.elements[1].start.y, 0);
    EXPECT_EQ(cleaned_shape.elements[1].end.x, 100);
    EXPECT_EQ(cleaned_shape.elements[1].end.y, 100);
    EXPECT_EQ(cleaned_shape.elements[2].start.x, 100);
    EXPECT_EQ(cleaned_shape.elements[2].start.y, 100);
    EXPECT_EQ(cleaned_shape.elements[2].end.x, 0);
    EXPECT_EQ(cleaned_shape.elements[2].end.y, 0);
}

TEST(IrregularShape, CleanShapeAligned2)
{
    Shape shape;

    ShapeElement element_1;
    element_1.type = ShapeElementType::LineSegment;
    element_1.start = {0, 0};
    element_1.end = {100, 0};
    shape.elements.push_back(element_1);

    ShapeElement element_2;
    element_2.type = ShapeElementType::LineSegment;
    element_2.start = {100, 0};
    element_2.end = {100, 100};
    shape.elements.push_back(element_2);

    ShapeElement element_3;
    element_3.type = ShapeElementType::LineSegment;
    element_3.start = {100, 100};
    element_3.end = {50, 50};
    shape.elements.push_back(element_3);

    ShapeElement element_4;
    element_4.type = ShapeElementType::LineSegment;
    element_4.start = {50, 50};
    element_4.end = {0, 0};
    shape.elements.push_back(element_4);

    Shape cleaned_shape = clean_shape(shape);

    EXPECT_EQ(cleaned_shape.elements.size(), 3);
    EXPECT_EQ(cleaned_shape.elements[0].start.x, 0);
    EXPECT_EQ(cleaned_shape.elements[0].start.y, 0);
    EXPECT_EQ(cleaned_shape.elements[0].end.x, 100);
    EXPECT_EQ(cleaned_shape.elements[0].end.y, 0);
    EXPECT_EQ(cleaned_shape.elements[1].start.x, 100);
    EXPECT_EQ(cleaned_shape.elements[1].start.y, 0);
    EXPECT_EQ(cleaned_shape.elements[1].end.x, 100);
    EXPECT_EQ(cleaned_shape.elements[1].end.y, 100);
    EXPECT_EQ(cleaned_shape.elements[2].start.x, 100);
    EXPECT_EQ(cleaned_shape.elements[2].start.y, 100);
    EXPECT_EQ(cleaned_shape.elements[2].end.x, 0);
    EXPECT_EQ(cleaned_shape.elements[2].end.y, 0);
}

TEST(IrregularShape, Borders1)
{
    Shape shape = build_polygon_shape({{2, 0}, {3, 1}, {0, 1}});
    std::vector<Shape> shape_borders = borders(shape);

    EXPECT_EQ(shape_borders.size(), 3);
}

TEST(IrregularShape, Borders2)
{
    Shape shape = build_polygon_shape({{0, 0}, {3, 1}, {0, 1}});
    std::vector<Shape> shape_borders = borders(shape);
    for (const Shape& border: shape_borders)
        std::cout << border.to_string(0) << std::endl;

    EXPECT_EQ(shape_borders.size(), 1);
}
