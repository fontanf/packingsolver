#include "shape_inflate.hpp"
#include "shape_closure.hpp"
#include "shape_self_intersections_removal.hpp"
#include <cmath>

namespace packingsolver
{
namespace irregular
{

// Check if an arc is covered by adjacent elements
bool is_arc_covered_by_adjacent_elements(
        const ShapeElement& prev_element,
        const ShapeElement& arc_element,
        const ShapeElement& next_element,
        LengthDbl value)
{
    // If the inflation value is zero or negative, it cannot be covered
    if (value <= 0) {
        return false;
    }
    
    // If adjacent elements are both line segments and the angle between them is small, they may cover the arc
    if (prev_element.type == ShapeElementType::LineSegment && 
        next_element.type == ShapeElementType::LineSegment) {
        
        // Calculate the direction vector of the previous segment
        LengthDbl dx1 = prev_element.end.x - prev_element.start.x;
        LengthDbl dy1 = prev_element.end.y - prev_element.start.y;
        LengthDbl length1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
        
        // Calculate the direction vector of the next segment
        LengthDbl dx2 = next_element.end.x - next_element.start.x;
        LengthDbl dy2 = next_element.end.y - next_element.start.y;
        LengthDbl length2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
        
        if (length1 > 0 && length2 > 0) {
            // Calculate unit direction vectors
            LengthDbl dir1_x = dx1 / length1;
            LengthDbl dir1_y = dy1 / length1;
            LengthDbl dir2_x = dx2 / length2;
            LengthDbl dir2_y = dy2 / length2;
            
            // Calculate dot product to determine the angle
            LengthDbl dot_product = dir1_x * dir2_x + dir1_y * dir2_y;
            
            // If the two segments are almost parallel (dot product close to 1 or -1), and the inflation value is large enough
            // This is a simplified check, an actual implementation would need more precise geometric calculations
            if (std::abs(std::abs(dot_product) - 1.0) < 0.1) {
                // Calculate the arc radius
                LengthDbl radius = std::sqrt(
                    std::pow(arc_element.start.x - arc_element.center.x, 2) + 
                    std::pow(arc_element.start.y - arc_element.center.y, 2)
                );
                
                // If the inflation value is greater than the radius, it may be covered
                return value > radius;
            }
        }
    }
    
    // By default, assume it's not covered
    return false;
}

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
    
    for (size_t i = 0; i < original_shape.elements.size(); ++i) {
        const ShapeElement& element = original_shape.elements[i];
        ShapeElement inflated_element = offset_element(element, value);
        if (!is_degenerate_element(inflated_element)) {
            inflated_elements.push_back(inflated_element);
        }
    }
    
    //std::cout << "Created " << inflated_elements.size() << " inflated elements" << std::endl;
    
    // For deflation (value < 0), remove intersections before closure
    // For inflation (value >= 0), also remove intersections before closure
    if (!inflated_elements.empty()) {
        //std::cout << "Removing intersections..." << std::endl;
        inflated_elements = remove_intersections_segments(inflated_elements, is_deflating);
        //std::cout << "After intersection removal: " << inflated_elements.size() << " elements" << std::endl;
    }
    
    // Close the shape (connect elements where needed)
    if (!inflated_elements.empty()) {
        //std::cout << "Closing inflated elements..." << std::endl;
        Shape inflated_shape = close_inflated_elements(inflated_elements, is_deflating);
        //std::cout << "Closed shape has " << inflated_shape.elements.size() << " elements" << std::endl;
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

    // 使用已改进的inflate_shape_without_holes函数处理主形状
    // (内部已实现元素间相交检测和闭合处理)
    Shape inflated_shape = inflate_shape_without_holes(shape, value);
    
    // 处理原始孔洞(使用相反的膨胀值)
    std::vector<Shape> deflated_holes;
    for (const Shape& hole : holes) {
        // 对孔洞形状进行收缩/膨胀处理(使用相反值)
        Shape deflated_hole = inflate_shape_without_holes(hole, -value);
        deflated_holes.push_back(deflated_hole);
    }
    
    // 返回膨胀后的形状和处理后的孔洞
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
        LengthDbl length = std::sqrt(dx * dx + dy * dy);
        
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
        LengthDbl radius = std::sqrt(
            std::pow(element.start.x - element.center.x, 2) + 
            std::pow(element.start.y - element.center.y, 2)
        );
        
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
        LengthDbl start_angle = std::atan2(
            element.start.y - element.center.y, 
            element.start.x - element.center.x
        );
        LengthDbl end_angle = std::atan2(
            element.end.y - element.center.y, 
            element.end.x - element.center.x
        );
        
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
        LengthDbl dx = element.end.x - element.start.x;
        LengthDbl dy = element.end.y - element.start.y;
        LengthDbl length = std::sqrt(dx * dx + dy * dy);
        return length < 1e-6;
    } else if (element.type == ShapeElementType::CircularArc) {
        LengthDbl radius = std::sqrt(
            std::pow(element.start.x - element.center.x, 2) + 
            std::pow(element.start.y - element.center.y, 2)
        );
        
        // If the radius is very small, consider it degenerate
        if (radius < 1e-6) {
            return true;
        }
        
        // If start and end points are too close, consider it degenerate
        LengthDbl dx = element.end.x - element.start.x;
        LengthDbl dy = element.end.y - element.start.y;
        LengthDbl chord_length = std::sqrt(dx * dx + dy * dy);
        return chord_length < 1e-6;
    }
    
    return false;
}

}
} 