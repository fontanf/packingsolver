#include "irregular/shape.hpp"
#include "irregular/shape_self_intersections_removal.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

// build square
Shape build_square(LengthDbl x, LengthDbl y, LengthDbl size)
{
    return build_polygon_shape({
        {x, y},
        {x + size, y},
        {x + size, y + size},
        {x, y + size}
    });
}

// build triangle
Shape build_triangle(LengthDbl x, LengthDbl y, LengthDbl size)
{
    return build_polygon_shape({
        {x, y},
        {x + size, y},
        {x + size / 2, y + size}
    });
}

// test 1: test basic inflation - no holes
TEST(IrregularShapeInflate, BasicInflate)
{
    // build a simple square
    Shape square = build_square(0, 0, 10);
    
    // inflate shape
    LengthDbl inflation_value = 2.0;
    Shape inflated_square = inflate(square, inflation_value);
    
    // verify the inflated shape
    // the square's edges should be moved outwards by inflation_value units
    EXPECT_EQ(inflated_square.elements.size(), square.elements.size() * 2);
    
    // expected new boundary coordinates
    LengthDbl expected_min_x = -inflation_value;
    LengthDbl expected_min_y = -inflation_value;
    LengthDbl expected_max_x = 10 + inflation_value;
    LengthDbl expected_max_y = 10 + inflation_value;
    
    // check if the inflated shape contains the expected external points
    bool contains_min_x = false;
    bool contains_min_y = false;
    bool contains_max_x = false;
    bool contains_max_y = false;
    
    for (const ShapeElement& element : inflated_square.elements) {
        if (element.start.x <= expected_min_x || element.end.x <= expected_min_x) {
            contains_min_x = true;
        }
        if (element.start.y <= expected_min_y || element.end.y <= expected_min_y) {
            contains_min_y = true;
        }
        if (element.start.x >= expected_max_x || element.end.x >= expected_max_x) {
            contains_max_x = true;
        }
        if (element.start.y >= expected_max_y || element.end.y >= expected_max_y) {
            contains_max_y = true;
        }
    }
    
    EXPECT_TRUE(contains_min_x);
    EXPECT_TRUE(contains_min_y);
    EXPECT_TRUE(contains_max_x);
    EXPECT_TRUE(contains_max_y);
}

// test 2: test inflation with holes
TEST(IrregularShapeInflate, InflateWithHoles)
{
    // build a square as the outer shape
    Shape outer_square = build_square(0, 0, 20);
    
    // build a small square as the hole
    Shape hole = build_square(5, 5, 10);
    
    // inflate the shape with the hole
    LengthDbl inflation_value = 2.0;
    Shape inflated_shape = inflate(outer_square, inflation_value, {hole});
    
    // verify the inflated shape
    // the outer boundary should be expanded, and the hole should be contracted
    
    // expected new outer boundary coordinates
    LengthDbl expected_outer_min_x = -inflation_value;
    LengthDbl expected_outer_min_y = -inflation_value;
    LengthDbl expected_outer_max_x = 20 + inflation_value;
    LengthDbl expected_outer_max_y = 20 + inflation_value;
    
    // verify the inflated shape
    bool contains_outer_min_x = false;
    bool contains_outer_min_y = false;
    bool contains_outer_max_x = false;
    bool contains_outer_max_y = false;
    
    for (const ShapeElement& element : inflated_shape.elements) {
        if (element.start.x <= expected_outer_min_x || element.end.x <= expected_outer_min_x) {
            contains_outer_min_x = true;
        }
        if (element.start.y <= expected_outer_min_y || element.end.y <= expected_outer_min_y) {
            contains_outer_min_y = true;
        }
        if (element.start.x >= expected_outer_max_x || element.end.x >= expected_outer_max_x) {
            contains_outer_max_x = true;
        }
        if (element.start.y >= expected_outer_max_y || element.end.y >= expected_outer_max_y) {
            contains_outer_max_y = true;
        }
    }
    
    EXPECT_TRUE(contains_outer_min_x);
    EXPECT_TRUE(contains_outer_min_y);
    EXPECT_TRUE(contains_outer_max_x);
    EXPECT_TRUE(contains_outer_max_y);
}

// test 3: test the case that may cause self-intersections
TEST(IrregularShapeInflate, SelfIntersectingInflate)
{
    // build a "U" shape, larger inflation will cause self-intersections between the two arms
    Shape u_shape = build_polygon_shape({
        {0, 0},
        {0, 10},
        {2, 10},
        {2, 2},
        {8, 2},
        {8, 10},
        {10, 10},
        {10, 0}
    });
    
    // use a larger inflation value, ensure self-intersections will be produced
    LengthDbl inflation_value = 2.0;
    Shape inflated_shape = inflate(u_shape, inflation_value);
    
    // verify the inflated shape has no self-intersections
    // first check if the self-intersections removal function works properly
    auto processed = remove_self_intersections(inflated_shape);
    Shape no_self_intersections = processed.first;
    std::vector<Shape> holes = processed.second;
    
    // after self-intersections removal, we should get a shape with no self-intersections and possible holes
    // U shape inflation will cause a hole in the middle
    EXPECT_FALSE(holes.empty()); // should at least produce one hole
    
    // verify the processed shape is still complete
    EXPECT_GT(no_self_intersections.elements.size(), 0);
}

// test 4: test shape deflation
TEST(IrregularShapeInflate, Deflate)
{
    // build a large square
    Shape square = build_square(0, 0, 20);
    
    // deflate shape (use negative inflation value)
    LengthDbl deflation_value = -5.0;
    Shape deflated_square = inflate(square, deflation_value);
    
    // verify the deflated shape
    // the square's edges should be moved inwards by |deflation_value| units
    
    // expected new boundary coordinates
    LengthDbl expected_min_x = -deflation_value;
    LengthDbl expected_min_y = -deflation_value;
    LengthDbl expected_max_x = 20 + deflation_value;
    LengthDbl expected_max_y = 20 + deflation_value;
    
    // check the boundary of the deflated shape
    LengthDbl actual_min_x = std::numeric_limits<LengthDbl>::max();
    LengthDbl actual_min_y = std::numeric_limits<LengthDbl>::max();
    LengthDbl actual_max_x = std::numeric_limits<LengthDbl>::lowest();
    LengthDbl actual_max_y = std::numeric_limits<LengthDbl>::lowest();
    
    for (const ShapeElement& element : deflated_square.elements) {
        actual_min_x = std::min({actual_min_x, element.start.x, element.end.x});
        actual_min_y = std::min({actual_min_y, element.start.y, element.end.y});
        actual_max_x = std::max({actual_max_x, element.start.x, element.end.x});
        actual_max_y = std::max({actual_max_y, element.start.y, element.end.y});
    }
    
    // allow some tolerance
    const LengthDbl tolerance = 0.01;
    
    EXPECT_NEAR(actual_min_x, expected_min_x, tolerance);
    EXPECT_NEAR(actual_min_y, expected_min_y, tolerance);
    EXPECT_NEAR(actual_max_x, expected_max_x, tolerance);
    EXPECT_NEAR(actual_max_y, expected_max_y, tolerance);
}

// test 5: test the case that the shape is too small
TEST(IrregularShapeInflate, TinyShape)
{
    // build a very small square
    Shape tiny_square = build_square(0, 0, 0.1);
    
    // inflate shape
    LengthDbl inflation_value = 0.5;
    Shape inflated_square = inflate(tiny_square, inflation_value);
    
    // verify the inflated shape
    EXPECT_GT(inflated_square.elements.size(), 0);
}

// test 6: test the case that the inflation value is too large
TEST(IrregularShapeInflate, LargeInflation)
{
    // build a square
    Shape square = build_square(0, 0, 10);
    
    // use a large inflation value
    LengthDbl inflation_value = 100.0;
    Shape inflated_square = inflate(square, inflation_value);
    
    // verify the inflated shape
    EXPECT_GT(inflated_square.elements.size(), 0);
    
    // after inflation, the square's edges should be moved outwards by inflation_value units
    // expected new boundary coordinates
    LengthDbl expected_min_x = -inflation_value;
    LengthDbl expected_min_y = -inflation_value;
    LengthDbl expected_max_x = 10 + inflation_value;
    LengthDbl expected_max_y = 10 + inflation_value;
    
    // check the boundary of the inflated shape
    LengthDbl actual_min_x = std::numeric_limits<LengthDbl>::max();
    LengthDbl actual_min_y = std::numeric_limits<LengthDbl>::max();
    LengthDbl actual_max_x = std::numeric_limits<LengthDbl>::lowest();
    LengthDbl actual_max_y = std::numeric_limits<LengthDbl>::lowest();
    
    for (const ShapeElement& element : inflated_square.elements) {
        actual_min_x = std::min({actual_min_x, element.start.x, element.end.x});
        actual_min_y = std::min({actual_min_y, element.start.y, element.end.y});
        actual_max_x = std::max({actual_max_x, element.start.x, element.end.x});
        actual_max_y = std::max({actual_max_y, element.start.y, element.end.y});
    }
    
    // allow some tolerance
    const LengthDbl tolerance = 0.1;
    
    EXPECT_NEAR(actual_min_x, expected_min_x, tolerance);
    EXPECT_NEAR(actual_min_y, expected_min_y, tolerance);
    EXPECT_NEAR(actual_max_x, expected_max_x, tolerance);
    EXPECT_NEAR(actual_max_y, expected_max_y, tolerance);
} 