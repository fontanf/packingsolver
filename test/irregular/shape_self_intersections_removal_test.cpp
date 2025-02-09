#include "irregular/shape_self_intersections_removal.hpp"

#include "irregular/shape.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

TEST(IrregularShapeSelfIntersectionRemoval, Shape1)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {2.5, 4},
            {2.5, 3},
            {3, 3},
            {3, 1},
            {1, 1},
            {1, 3},
            {1.5, 3},
            {1.5, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = shape;
    std::vector<Shape> expected_holes = {};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionRemoval, Shape2)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 3},
            {3, 3},
            {1, 4},
            {3, 1},
            {1, 1},
            {3, 4},
            {1, 3},
            {0, 3}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 3},
            {3, 3},
            {2.5, 3.25},
            {3, 4},
            {2, 3.5},
            {1, 4},
            {1.5, 3.25},
            {1, 3},
            {0, 3}});
    std::vector<Shape> expected_holes = {
        build_polygon_shape({
                {1, 1},
                {3, 1},
                {2, 2.5}})};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionRemoval, Shape3)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {1, 4},
            {3, 3},
            {3, 1},
            {1, 1},
            {1, 3},
            {3, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {0, 4} });
    std::vector<Shape> expected_holes = {
        build_polygon_shape({
                {1, 1},
                {3, 1},
                {3, 3},
                {2, 3.5},
                {1, 3}})};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionRemoval, Shape4)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {2, 4},
            {3, 3},
            {3, 1},
            {1, 1},
            {1, 3},
            {2, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {0, 4} });
    std::vector<Shape> expected_holes = {
        build_polygon_shape({
                {1, 1},
                {3, 1},
                {3, 3},
                {2, 4},
                {1, 3}})};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionRemoval, Shape5)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {2, 4},
            {3, 3},
            {3, 1},
            {2, 4},
            {1, 1},
            {1, 3},
            {2, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {0, 4} });
    std::vector<Shape> expected_holes = {
        build_polygon_shape({
                {3, 1},
                {3, 3},
                {2, 4}}),
        build_polygon_shape({
                {1, 1},
                {2, 4},
                {1, 3}})};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionRemoval, Shape6)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {2, 4},
            {2, 3},
            {3, 3},
            {3, 1},
            {1, 1},
            {1, 3},
            {2, 3},
            {2, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {0, 4} });
    std::vector<Shape> expected_holes = {
        build_polygon_shape({
                {1, 1},
                {3, 1},
                {3, 3},
                {1, 3}})};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionRemoval, Shape7)
{
    Shape shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {1, 4},
            {1, 1},
            {3, 1},
            {3, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    Shape expected_shape = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {0, 4} });
    std::vector<Shape> expected_holes = {};

    auto p = remove_self_intersections(shape);

    std::cout << "shape: " << p.first.to_string(0) << std::endl;
    //p.first.write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << p.second[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    EXPECT_EQ(p.first, expected_shape);
    ASSERT_EQ(p.second.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)p.second.size();
            ++hole_pos) {
        EXPECT_EQ(p.second[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionExtractAllHoles, Shape1)
{
    Shape hole = build_polygon_shape({
            {0, 0},
            {4, 0},
            {4, 4},
            {2.5, 4},
            {2.5, 3},
            {3, 3},
            {3, 1},
            {1, 1},
            {1, 3},
            {1.5, 3},
            {1.5, 4},
            {0, 4}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    std::vector<Shape> expected_holes = {hole};

    auto holes = extract_all_holes_from_self_intersecting_hole(hole);

    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)holes.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << holes[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    ASSERT_EQ(holes.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)holes.size();
            ++hole_pos) {
        EXPECT_EQ(holes[hole_pos], expected_holes[hole_pos]);
    }
}

TEST(IrregularShapeSelfIntersectionExtractAllHoles, Shape2)
{
    Shape hole = build_polygon_shape({
            {0, 0},
            {4, 0},
            {0, 2},
            {4, 4},
            {0, 4},
            {4, 2}});
    //shape.write_svg("irregular_shape_self_intersection_removal_test_1.svg");

    std::vector<Shape> expected_holes = {
        build_polygon_shape({
                {0, 0},
                {4, 0},
                {2, 1}}),
        build_polygon_shape({
                {2, 3},
                {4, 4},
                {0, 4}})};

    auto holes = extract_all_holes_from_self_intersecting_hole(hole);

    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)holes.size();
            ++hole_pos) {
        std::cout << "hole " << hole_pos << ": " << holes[hole_pos].to_string(0) << std::endl;
        //p.second[hole_pos].write_svg("irregular_shape_self_intersection_removal_test_1.svg");
    }

    ASSERT_EQ(holes.size(), expected_holes.size());
    for (ShapePos hole_pos = 0;
            hole_pos < (ShapePos)holes.size();
            ++hole_pos) {
        EXPECT_EQ(holes[hole_pos], expected_holes[hole_pos]);
    }
}
