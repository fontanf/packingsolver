#include "packingsolver/irregular/shape.hpp"
#include "irregular/shape_inflate.hpp"
#include "irregular/shape_self_intersections_removal.hpp"

#include <gtest/gtest.h>
#include <iostream>

using namespace packingsolver;
using namespace packingsolver::irregular;

// build square
Shape build_square(LengthDbl x, LengthDbl y, LengthDbl size)
{
    return build_shape({
        {x, y},                // bottom-left
        {x + size, y},         // bottom-right
        {x + size, y + size},  // top-right
        {x, y + size}          // top-left
    });
}

// build rounded square (for inflated shapes)
Shape build_rounded_square(LengthDbl x, LengthDbl y, LengthDbl width, LengthDbl height, LengthDbl radius)
{
    // Directly create Shape object and its elements
    Shape shape;

    // Element 0: Left edge - LineSegment
    ShapeElement left;
    left.type = ShapeElementType::LineSegment;
    left.start = {x, y + height - radius};
    left.end = {x, y + radius};
    shape.elements.push_back(left);
    
    // Element 1: Bottom-left corner - CircularArc
    ShapeElement bottom_left_corner;
    bottom_left_corner.type = ShapeElementType::CircularArc;
    bottom_left_corner.start = {x, y + radius};
    bottom_left_corner.end = {x + radius, y};
    bottom_left_corner.center = {x + radius, y + radius};
    bottom_left_corner.anticlockwise = true; // Counterclockwise
    shape.elements.push_back(bottom_left_corner);
    
    // Element 2: Bottom edge - LineSegment
    ShapeElement bottom;
    bottom.type = ShapeElementType::LineSegment;
    bottom.start = {x + radius, y};
    bottom.end = {x + width - radius, y};
    shape.elements.push_back(bottom);
    
    // Element 3: Bottom-right corner - CircularArc
    ShapeElement bottom_right_corner;
    bottom_right_corner.type = ShapeElementType::CircularArc;
    bottom_right_corner.start = {x + width - radius, y};
    bottom_right_corner.end = {x + width, y + radius};
    bottom_right_corner.center = {x + width - radius, y + radius};
    bottom_right_corner.anticlockwise = true; // Counterclockwise
    shape.elements.push_back(bottom_right_corner);
    
    // Element 4: Right edge - LineSegment
    ShapeElement right;
    right.type = ShapeElementType::LineSegment;
    right.start = {x + width, y + radius};
    right.end = {x + width, y + height - radius};
    shape.elements.push_back(right);
    
    // Element 5: Top-right corner - CircularArc
    ShapeElement top_right_corner;
    top_right_corner.type = ShapeElementType::CircularArc;
    top_right_corner.start = {x + width, y + height - radius};
    top_right_corner.end = {x + width - radius, y + height};
    top_right_corner.center = {x + width - radius, y + height - radius};
    top_right_corner.anticlockwise = true; // Counterclockwise
    shape.elements.push_back(top_right_corner);
    
    // Element 6: Top edge - LineSegment
    ShapeElement top;
    top.type = ShapeElementType::LineSegment;
    top.start = {x + width - radius, y + height};
    top.end = {x + radius, y + height};
    shape.elements.push_back(top);
    
    // Element 7: Top-left corner - CircularArc
    ShapeElement top_left_corner;
    top_left_corner.type = ShapeElementType::CircularArc;
    top_left_corner.start = {x + radius, y + height};
    top_left_corner.end = {x, y + height - radius};
    top_left_corner.center = {x + radius, y + height - radius};
    top_left_corner.anticlockwise = true; // Counterclockwise
    shape.elements.push_back(top_left_corner);
    
    return shape;
}

// build triangle
Shape build_triangle(LengthDbl x, LengthDbl y, LengthDbl size)
{
    return build_shape({
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
    auto inflation_result = inflate(square, inflation_value);
    Shape inflated_square = inflation_result.first;
    
    // verify the inflated shape
    // the square's edges should be moved outwards by inflation_value units
    
    // Check element count - inflated square becomes a rounded rectangle
    // 4 edges + 4 circular arcs
    EXPECT_EQ(inflated_square.elements.size(), 8);
    
    // Build the expected inflated rounded rectangle shape
    Shape expected_inflated_square = build_rounded_square(
        -inflation_value, -inflation_value,
        10 + 2 * inflation_value, 10 + 2 * inflation_value,
        inflation_value);
    

    
    // Debug output: print each element
    std::cout << "\nActual shape elements:\n";
    for (size_t i = 0; i < inflated_square.elements.size(); ++i) {
        const auto& element = inflated_square.elements[i];
        std::cout << "Element " << i << ": ";
        if (element.type == ShapeElementType::CircularArc) {
            std::cout << "CircularArc start (" << element.start.x << ", " << element.start.y 
                      << ") end (" << element.end.x << ", " << element.end.y 
                      << ") center (" << element.center.x << ", " << element.center.y 
                      << ") " << (element.anticlockwise ? "anticlockwise" : "clockwise") << "\n";
        } else {
            std::cout << "LineSegment start (" << element.start.x << ", " << element.start.y 
                      << ") end (" << element.end.x << ", " << element.end.y << ")\n";
        }
    }
    
    std::cout << "\nExpected shape elements:\n";
    for (size_t i = 0; i < expected_inflated_square.elements.size(); ++i) {
        const auto& element = expected_inflated_square.elements[i];
        std::cout << "Element " << i << ": ";
        if (element.type == ShapeElementType::CircularArc) {
            std::cout << "CircularArc start (" << element.start.x << ", " << element.start.y 
                      << ") end (" << element.end.x << ", " << element.end.y 
                      << ") center (" << element.center.x << ", " << element.center.y 
                      << ") " << (element.anticlockwise ? "anticlockwise" : "clockwise") << "\n";
        } else {
            std::cout << "LineSegment start (" << element.start.x << ", " << element.start.y 
                      << ") end (" << element.end.x << ", " << element.end.y << ")\n";
        }
    }
    
    // Compare shapes element by element using the near() function
    ASSERT_EQ(inflated_square.elements.size(), expected_inflated_square.elements.size());

    for (size_t i = 0; i < inflated_square.elements.size(); ++i) {
        EXPECT_TRUE(near(inflated_square.elements[i], expected_inflated_square.elements[i]));
    }
    
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
    auto inflation_result = inflate(outer_square, inflation_value, {hole});
    Shape inflated_shape = inflation_result.first;
    std::vector<Shape> inflated_holes = inflation_result.second;
    
    // Check the external shape after inflation
    // Build the expected external inflated rounded rectangle shape
    Shape expected_inflated_outer = build_rounded_square(
        -inflation_value, -inflation_value,
        20 + 2 * inflation_value, 20 + 2 * inflation_value,
        inflation_value);
    
    // Compare external shape element by element using the near() function
    ASSERT_EQ(inflated_shape.elements.size(), expected_inflated_outer.elements.size());
    for (size_t i = 0; i < inflated_shape.elements.size(); ++i) {
        EXPECT_TRUE(near(inflated_shape.elements[i], expected_inflated_outer.elements[i]));
    }
    
    // Verify the hole shape - should still be a square after contraction
    EXPECT_FALSE(inflated_holes.empty());
    if (!inflated_holes.empty()) {
        // Contracted hole should still be a square, not rounded
        Shape expected_inflated_hole = build_square(
            5 + inflation_value, 
            5 + inflation_value, 
            10 - 2 * inflation_value);
            
        // Compare hole shape element by element using the near() function
        ASSERT_EQ(inflated_holes[0].elements.size(), expected_inflated_hole.elements.size());
        for (size_t i = 0; i < inflated_holes[0].elements.size(); ++i) {
            EXPECT_TRUE(near(inflated_holes[0].elements[i], expected_inflated_hole.elements[i]));
        }
    }
    
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
    
    // Also verify the inflated holes if needed
    EXPECT_FALSE(inflated_holes.empty());
}

// test 3: test shape deflation
TEST(IrregularShapeInflate, Deflate)
{
    // build a large square
    Shape square = build_square(0, 0, 20);
    
    // deflate shape (use negative inflation value)
    LengthDbl deflation_value = -5.0;
    auto inflation_result = inflate(square, deflation_value);
    Shape deflated_square = inflation_result.first;
    
    // Build the expected contracted rectangle shape (remains rectangular, not rounded)
    Shape expected_deflated_square = build_square(
        -deflation_value, 
        -deflation_value, 
        20 + 2 * deflation_value);
    
    // Compare shapes element by element using the near() function
    ASSERT_EQ(deflated_square.elements.size(), expected_deflated_square.elements.size());
    for (size_t i = 0; i < deflated_square.elements.size(); ++i) {
        EXPECT_TRUE(near(deflated_square.elements[i], expected_deflated_square.elements[i]));
    }
    
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

// test 4: test the case that the shape is too small
TEST(IrregularShapeInflate, TinyShape)
{
    // build a very small square
    Shape tiny_square = build_square(0, 0, 0.1);
    
    // inflate shape
    LengthDbl inflation_value = 0.5;
    auto inflation_result = inflate(tiny_square, inflation_value);
    Shape inflated_square = inflation_result.first;
    
    // Build the expected inflated rounded rectangle shape
    Shape expected_inflated_square = build_rounded_square(
        -inflation_value, -inflation_value,
        0.1 + 2 * inflation_value, 0.1 + 2 * inflation_value,
        inflation_value);
    
    // Compare shapes element by element using the near() function
    ASSERT_EQ(inflated_square.elements.size(), expected_inflated_square.elements.size());
    for (size_t i = 0; i < inflated_square.elements.size(); ++i) {
        EXPECT_TRUE(near(inflated_square.elements[i], expected_inflated_square.elements[i]));
    }
    
    // verify the inflated shape
    EXPECT_GT(inflated_square.elements.size(), 0);
}

// test 5: test the case that the inflation value is too large
TEST(IrregularShapeInflate, LargeInflation)
{
    // build a square
    Shape square = build_square(0, 0, 10);
    
    // use a large inflation value
    LengthDbl inflation_value = 100.0;
    auto inflation_result = inflate(square, inflation_value);
    Shape inflated_square = inflation_result.first;
    
    // Build the expected inflated rounded rectangle shape
    Shape expected_inflated_square = build_rounded_square(
        -inflation_value, -inflation_value,
        10 + 2 * inflation_value, 10 + 2 * inflation_value,
        inflation_value);
    
    // Compare shapes element by element using the near() function
    ASSERT_EQ(inflated_square.elements.size(), expected_inflated_square.elements.size());
    for (size_t i = 0; i < inflated_square.elements.size(); ++i) {
        EXPECT_TRUE(near(inflated_square.elements[i], expected_inflated_square.elements[i]));
    }
    
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