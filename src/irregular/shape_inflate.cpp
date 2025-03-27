#include "packingsolver/irregular/shape.hpp"
#include "irregular/shape_inflate.hpp"
#include "irregular/shape_closure.hpp"
#include "irregular/shape_self_intersections_removal.hpp"
#include <cmath>

// Define M_PI (if not provided by the system)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace packingsolver
{
namespace irregular
{

Shape inflate_shape_without_holes(
        const Shape& original_shape,
        LengthDbl value)
{
    if (original_shape.elements.empty()) {
        return original_shape;
    }
    
    bool is_deflating = value < 0;
    
    //std::cout << "\n------ Debug: inflate_shape_without_holes ------" << std::endl;
    //std::cout << "Inflation value: " << value << std::endl;
    //std::cout << "Is deflating: " << (is_deflating ? "true" : "false") << std::endl;
    
    // To inflate a shape, we need to:
    // 1. Offset all elements by the inflation value
    // 2. Handle intersections (intersections should be removed)
    // 3. Close any gaps (e.g., at corners)
    
    // Create inflated elements by offsetting each element
    std::vector<ShapeElement> inflated_elements;
    inflated_elements.reserve(original_shape.elements.size());
    
    // Store the mapping between original shape elements and inflated elements
    std::vector<std::pair<ShapeElement, ShapeElement>> original_to_inflated_mapping;
    original_to_inflated_mapping.reserve(original_shape.elements.size());
    
    // std::cout << "\n==== Debug: Creating Shape Inflation Mapping ====" << std::endl;
    // std::cout << "Original shape element count: " << original_shape.elements.size() << std::endl;
    
    for (ElementPos element_pos = 0; element_pos < (ElementPos)original_shape.elements.size(); ++element_pos) {
        const ShapeElement& element = original_shape.elements[element_pos];
        ShapeElement inflated_element = offset_element(element, value);
        
        // std::cout << "Original element " << i << ": ";
        // if (element.type == ShapeElementType::LineSegment) {
        //     std::cout << "Line Segment";
        // } else {
        //     std::cout << "Circular Arc";
        // }
        // std::cout << " start(" << element.start.x << ", " << element.start.y 
        //           << "), end(" << element.end.x << ", " << element.end.y << ")";
        
        // if (element.type == ShapeElementType::CircularArc) {
        //     std::cout << ", center(" << element.center.x << ", " << element.center.y 
        //               << "), " << (element.anticlockwise ? "counterclockwise" : "clockwise");
        // }
        std::cout << std::endl;
        
        if (!is_degenerate_element(inflated_element)) {
            inflated_elements.push_back(inflated_element);
            original_to_inflated_mapping.push_back({element, inflated_element});
            
            // std::cout << "  Inflated element: ";
            // if (inflated_element.type == ShapeElementType::LineSegment) {
            //     std::cout << "Line Segment";
            // } else {
            //     std::cout << "Circular Arc";
            // }
            // std::cout << " start(" << inflated_element.start.x << ", " << inflated_element.start.y 
            //           << "), end(" << inflated_element.end.x << ", " << inflated_element.end.y << ")";
            
            // if (inflated_element.type == ShapeElementType::CircularArc) {
            //     std::cout << ", center(" << inflated_element.center.x << ", " << inflated_element.center.y 
            //               << "), " << (inflated_element.anticlockwise ? "counterclockwise" : "clockwise");
            // }
            // std::cout << std::endl;
        } else {
            // std::cout << "  This element is degenerate after inflation, ignored" << std::endl;
        }
    }
    
    //std::cout << "Created " << inflated_elements.size() << " inflated elements" << std::endl;
    // std::cout << "Inflated element count: " << inflated_elements.size() << std::endl;
    // std::cout << "Mapping relationship count: " << original_to_inflated_mapping.size() << std::endl;
    
    // For deflation (value < 0), remove intersections before closure
    // For inflation (value >= 0), also remove intersections before closure
    if (!inflated_elements.empty()) {
        //std::cout << "Removing intersections..." << std::endl;
        // std::cout << "\n==== Debug: Removing Intersections ====" << std::endl;
        
        // If there are intersection points, regenerate the mapping relationship
        auto new_inflated_elements = remove_intersections_segments(inflated_elements, is_deflating);
        
        // std::cout << "Element count before intersection removal: " << inflated_elements.size() << std::endl;
        // std::cout << "Element count after intersection removal: " << new_inflated_elements.size() << std::endl;
        
        // Even if the shape elements change after processing, keep the mapping relationship
        // The intersection point processing may change the number of elements, but the original non-tangent points are still valid reference points
        // No longer clear the mapping relationship, let the subsequent closure process try to find the original non-tangent points
        /*
        if (new_inflated_elements.size() != inflated_elements.size()) {
            original_to_inflated_mapping.clear();
        }
        */
        
        inflated_elements = new_inflated_elements;
        //std::cout << "After intersection removal: " << inflated_elements.size() << " elements" << std::endl;
    }
    
    // Close the shape (connect elements where needed)
    if (!inflated_elements.empty()) {
        //std::cout << "Closing inflated elements..." << std::endl;
        // std::cout << "\n==== Debug: Closing Shape ====" << std::endl;
        // std::cout << "Mapping relationship count before closing: " << original_to_inflated_mapping.size() << std::endl;
        
        Shape inflated_shape = close_inflated_elements(inflated_elements, original_to_inflated_mapping, is_deflating);
        //std::cout << "Closed shape has " << inflated_shape.elements.size() << " elements" << std::endl;
        // std::cout << "Element count after closing: " << inflated_shape.elements.size() << std::endl;
        
        return inflated_shape;
    }
    
    return original_shape;  // Return original shape if inflation failed
}

std::pair<Shape, std::vector<Shape>> inflate(
        const Shape& shape,
        LengthDbl value,
        const std::vector<Shape>& holes)
{
    if (shape.elements.empty()) {
        return {shape, {}};
    }

    // use the improved inflate_shape_without_holes function to process the main shape
    // (internal implementation of intersection detection and closure processing)
    Shape inflated_shape = inflate_shape_without_holes(shape, value);
    
    // process the original holes (using the opposite inflation value)
    std::vector<Shape> deflated_holes;
    for (const Shape& hole : holes) {
        // process the original holes (using the opposite inflation value)
        Shape deflated_hole = inflate_shape_without_holes(hole, -value);
        deflated_holes.push_back(deflated_hole);
    }
    
    // return the inflated shape and processed holes
    return {inflated_shape, deflated_holes};
}

// Helper function to offset an element by a given value
ShapeElement offset_element(const ShapeElement& element, LengthDbl value)
{
    ShapeElement new_element;
    
    // Process based on element type
    if (element.type == ShapeElementType::LineSegment) {
        // LineSegment processing
        LengthDbl dx = element.end.x - element.start.x;
        LengthDbl dy = element.end.y - element.start.y;
        LengthDbl length = distance(element.start, element.end);
        
        // Avoid division by zero
        if (length > 0) {
            // Calculate normal vector - always pointing outward from the shape
            LengthDbl nx = dy / length;
            LengthDbl ny = -dx / length;
            
            // Create new line segment
            new_element.type = ShapeElementType::LineSegment;
            new_element.start = Point{
                element.start.x + nx * value,
                element.start.y + ny * value
            };
            new_element.end = Point{
                element.end.x + nx * value,
                element.end.y + ny * value
            };
            
            //std::cout << "Offset LineSegment: normal=(" << nx << "," << ny 
            //          << "), start=(" << new_element.start.x << "," << new_element.start.y 
            //          << "), end=(" << new_element.end.x << "," << new_element.end.y << ")" << std::endl;
        }
    } else if (element.type == ShapeElementType::CircularArc) {
        // CircularArc processing
        
        // Calculate the current radius of the arc
        LengthDbl radius = distance(element.center, element.start);
        
        // Calculate new radius based on arc direction
        LengthDbl new_radius;
        if (element.anticlockwise) {
            // If anticlockwise (convex), positive value increases radius (expands),
            // negative value decreases radius (contracts)
            new_radius = radius + value;
        } else {
            // If clockwise (concave), positive value decreases radius (contracts),
            // negative value increases radius (expands)
            new_radius = radius - value;
        }
        
        // Skip if radius becomes too small
        if (new_radius <= 0.01) {
            return new_element; // Return empty/invalid element
        }
        
        // Calculate the original arc's start and end angles
        LengthDbl start_angle = angle_radian({element.start.x - element.center.x, element.start.y - element.center.y});
        LengthDbl end_angle = angle_radian({element.end.x - element.center.x, element.end.y - element.center.y});
        
        // Ensure angles are in the correct range
        if (element.anticlockwise && end_angle <= start_angle) {
            end_angle += 2 * M_PI;
        } else if (!element.anticlockwise && start_angle <= end_angle) {
            start_angle += 2 * M_PI;
        }
        
        // Create the inflated arc
        new_element.type = ShapeElementType::CircularArc;
        new_element.center = element.center; // Keep the center unchanged
        new_element.anticlockwise = element.anticlockwise; // Keep rotation direction unchanged
        
        // Calculate new start and end points
        new_element.start = Point{
            element.center.x + new_radius * std::cos(start_angle),
            element.center.y + new_radius * std::sin(start_angle)
        };
        new_element.end = Point{
            element.center.x + new_radius * std::cos(end_angle),
            element.center.y + new_radius * std::sin(end_angle)
        };
        
        //std::cout << "Offset CircularArc: center=(" << new_element.center.x << "," << new_element.center.y 
        //          << "), radius=" << new_radius
        //          << ", start=(" << new_element.start.x << "," << new_element.start.y 
        //          << "), end=(" << new_element.end.x << "," << new_element.end.y 
        //          << "), anticlockwise=" << (new_element.anticlockwise ? "true" : "false") << std::endl;
    }
    
    return new_element;
}

// Helper function to check if an element is degenerate (too small)
bool is_degenerate_element(const ShapeElement& element)
{
    if (element.type == ShapeElementType::LineSegment) {
        LengthDbl length = distance(element.start, element.end);
        return equal(length, 0.0);
    } else if (element.type == ShapeElementType::CircularArc) {
        LengthDbl radius = distance(element.center, element.start);
        
        // If the radius is very small, consider it degenerate
        if (equal(radius, 0.0)) {
            return true;
        }
        
        // If start and end points are too close, consider it degenerate
        LengthDbl chord_length = distance(element.start, element.end);
        return equal(chord_length, 0.0);
    }
    
    return false;
}

}
} 