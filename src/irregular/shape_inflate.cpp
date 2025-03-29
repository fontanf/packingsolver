#include "packingsolver/irregular/shape.hpp"
#include "irregular/shape_inflate.hpp"
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

// Determine vertex type (convex/concave/regular)
enum class VertexType {
    Convex,   // Convex vertex (outer angle < 180 degrees)
    Concave,  // Concave vertex (outer angle > 180 degrees)
    Regular   // Non-corner vertex
};

// Detect vertex type by analyzing adjacent elements
VertexType detect_vertex_type(const Point& prev_end, const Point& curr_start, const Point& curr_end) {
    // Ensure the current point is a corner point
    if (equal(prev_end, curr_start) && !equal(prev_end, curr_end)) {
        // Calculate vectors
        Point v1 = {prev_end.x - curr_start.x, prev_end.y - curr_start.y};
        Point v2 = {curr_end.x - curr_start.x, curr_end.y - curr_start.y};
        
        // Use cross product to determine vertex type
        LengthDbl cross = cross_product(v1, v2);
        
        if (std::abs(cross) < 1e-10) {
            return VertexType::Regular;  // Collinear point
        } else if (cross > 0) {
            return VertexType::Convex;   // Convex vertex (outer angle < 180 degrees)
        } else {
            return VertexType::Concave;  // Concave vertex (outer angle > 180 degrees)
        }
    }
    
    return VertexType::Regular;
}

Shape close_inflated_elements(
        const std::vector<ShapeElement>& inflated_elements, 
        const std::vector<std::pair<ShapeElement, ShapeElement>>& original_to_inflated_mapping,
        bool is_deflating)
{
    if (inflated_elements.empty()) {
        return Shape();
    }
    
    //std::cout << "\n------ Debug: close_inflated_elements ------" << std::endl;
    //std::cout << "Is deflating: " << (is_deflating ? "true" : "false") << std::endl;
    //std::cout << "Inflated elements: " << inflated_elements.size() << std::endl;
    
    // Create a new elements array with the first element copied to the end to ensure closure
    std::vector<ShapeElement> closed_elements = inflated_elements;
    if (inflated_elements.size() > 1) {
        closed_elements.push_back(inflated_elements[0]);
    }
    
    Shape inflated_shape;
    inflated_shape.elements.reserve(closed_elements.size() * 2); // Reserve space for connector elements
    
    // Iterate through elements (excluding the last copied element)
    for (size_t i = 0; i < closed_elements.size() - 1; ++i) {
        const ShapeElement& current = closed_elements[i];
        const ShapeElement& next = closed_elements[i + 1];
        
        // Add the current element
        inflated_shape.elements.push_back(current);
        
        //std::cout << "Processing element " << i << " to " << (i+1) << std::endl;
        //std::cout << "  Current: type=" << (current.type == ShapeElementType::LineSegment ? "LineSegment" : "CircularArc") 
        //          << ", start=(" << current.start.x << "," << current.start.y 
        //          << "), end=(" << current.end.x << "," << current.end.y << ")" << std::endl;
        //std::cout << "  Next: type=" << (next.type == ShapeElementType::LineSegment ? "LineSegment" : "CircularArc") 
        //          << ", start=(" << next.start.x << "," << next.start.y 
        //          << "), end=(" << next.end.x << "," << next.end.y << ")" << std::endl;
        
        // Check if a connector element is needed
        if (!equal(current.end, next.start)) {
            // The end point of the current element and the start point of the next element don't coincide, need to add a connector
            //std::cout << "  Need connector (gap > 1e-6)" << std::endl;
            ShapeElement connector;
            connector.type = ShapeElementType::CircularArc;
            connector.start = current.end;
            connector.end = next.start;
            const ShapeElement& current_orig_elem = original_to_inflated_mapping[i].first;
            connector.center = current_orig_elem.end;
            if (is_deflating) {
                // for deflation, add a clockwise circular arc connector on corner points
                connector.anticlockwise = false;
            } else {
                // For inflation, add a anti-clockwise circular arc connector on corner points
                connector.anticlockwise = true;
            }
            inflated_shape.elements.push_back(connector);
        }
    }
    
    // Remove duplicate points or degenerate elements
    if (inflated_shape.elements.size() > 1) {
        std::vector<ShapeElement> cleaned_elements;
        cleaned_elements.reserve(inflated_shape.elements.size());
        
        for (const ShapeElement& element : inflated_shape.elements) {
            if (!equal(element.start, element.end)) {
                cleaned_elements.push_back(element);
            }
        }
        
        inflated_shape.elements = cleaned_elements;
    }
    
    return inflated_shape;
}

Shape inflate_shape_without_holes(
        const Shape& original_shape,
        LengthDbl value)
{
    if (original_shape.elements.empty()) {
        return original_shape;
    }
    
    bool is_deflating = value < 0;
    
    // Create new shape
    Shape inflated_shape;
    std::vector<ShapeElement> temp_elements;
    temp_elements.reserve(original_shape.elements.size() * 2); // Reserve space for potential additional arcs
    
    // If shape is too small for deflation, return empty shape
    if (is_deflating) {
        // Calculate minimum edge length or radius to ensure shape won't disappear
        LengthDbl min_size = std::numeric_limits<LengthDbl>::max();
        for (const auto& element : original_shape.elements) {
            if (element.type == ShapeElementType::LineSegment) {
                min_size = std::min(min_size, distance(element.start, element.end));
            } else if (element.type == ShapeElementType::CircularArc) {
                min_size = std::min(min_size, distance(element.center, element.start));
            }
        }
        
        // If deflation value is too large, return empty shape
        if (std::abs(value) >= min_size / 2) {
            return Shape();
        }
    }
    
    // Step 1: Detect vertex types for all vertices
    std::vector<VertexType> vertex_types;
    vertex_types.reserve(original_shape.elements.size());
    
    // Pre-process closed shape special cases
    const size_t num_elements = original_shape.elements.size();
    for (size_t i = 0; i < num_elements; ++i) {
        const ShapeElement& curr_element = original_shape.elements[i];
        const ShapeElement& prev_element = original_shape.elements[(i + num_elements - 1) % num_elements];
        
        // Detect current vertex type
        VertexType vtype = detect_vertex_type(prev_element.end, curr_element.start, curr_element.end);
        vertex_types.push_back(vtype);
    }
    
    // Step 2: Process each element for inflation/deflation
    for (size_t i = 0; i < num_elements; ++i) {
        const ShapeElement& element = original_shape.elements[i];
        const VertexType curr_vertex_type = vertex_types[i];
        const VertexType next_vertex_type = vertex_types[(i + 1) % num_elements];
        
        // Offset current element
        ShapeElement offset_elem = offset_element(element, value);
        
        // If element is valid, add to temporary elements list
        if (!is_degenerate_element(offset_elem)) {
            temp_elements.push_back(offset_elem);
            
            // Based on next vertex type, decide whether to add connecting arc
            if (next_vertex_type != VertexType::Regular) {
                bool need_arc = false;
                bool arc_anticlockwise = true;
                
                if (is_deflating) {
                    // For deflation, add clockwise arc at concave points
                    if (next_vertex_type == VertexType::Concave) {
                        need_arc = true;
                        arc_anticlockwise = false; // Clockwise
                    }
                } else {
                    // For inflation, add counterclockwise arc at convex points
                    if (next_vertex_type == VertexType::Convex) {
                        need_arc = true;
                        arc_anticlockwise = true; // Counterclockwise
                    }
                }
                
                // If need to add arc
                if (need_arc) {
                    // Get next offset element
                    const ShapeElement& next_element = original_shape.elements[(i + 1) % num_elements];
                    ShapeElement next_offset = offset_element(next_element, value);
                    
                    // Check next element is degenerate
                    if (is_degenerate_element(next_offset)) {
                        // Find next non-degenerate element
                        size_t next_valid_idx = (i + 1) % num_elements;
                        ShapeElement next_valid_offset;
                        const ShapeElement* next_valid_element = nullptr;
                        bool found_valid = false;
                        
                        // Search backward until find a non-degenerate element
                        for (size_t j = 1; j < num_elements; ++j) {
                            next_valid_idx = (i + j) % num_elements;
                            const ShapeElement& candidate = original_shape.elements[next_valid_idx];
                            next_valid_offset = offset_element(candidate, value);
                            
                            if (!is_degenerate_element(next_valid_offset)) {
                                found_valid = true;
                                next_valid_element = &candidate;
                                break;
                            }
                        }
                        
                        // If no valid next element, skip arc addition
                        if (!found_valid) {
                            continue;
                        }
                        
                        // Calculate vector from current point to next valid element start point
                        Point direction = {
                            next_valid_element->start.x - element.end.x,
                            next_valid_element->start.y - element.end.y
                        };
                        LengthDbl dir_length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                        if (dir_length < 1e-10) continue;  // If vector too short, skip
                        
                        // Calculate normal direction (right-hand normal)
                        Point normal = {
                            -direction.y / dir_length,
                            direction.x / dir_length
                        };
                        
                        // Calculate first arc end point (intersection of ray and circle)
                        Point arc1_end = {
                            element.end.x + std::abs(value) * normal.x,
                            element.end.y + std::abs(value) * normal.y
                        };
                        
                        // Add first arc
                        ShapeElement arc1;
                        arc1.type = ShapeElementType::CircularArc;
                        arc1.start = offset_elem.end;
                        arc1.end = arc1_end;
                        arc1.center = element.end;
                        arc1.anticlockwise = arc_anticlockwise;
                        
                        // Add second arc
                        ShapeElement arc2;
                        arc2.type = ShapeElementType::CircularArc;
                        arc2.start = arc1_end;
                        arc2.end = next_valid_offset.start;
                        arc2.center = next_valid_element->start;
                        arc2.anticlockwise = arc_anticlockwise;
                        
                        // Only add arc if it's not degenerate
                        if (!is_degenerate_element(arc1)) {
                            temp_elements.push_back(arc1);
                        }
                        if (!is_degenerate_element(arc2)) {
                            temp_elements.push_back(arc2);
                        }
                    } else {
                        // Normal processing logic
                        ShapeElement arc;
                        arc.type = ShapeElementType::CircularArc;
                        arc.start = offset_elem.end;
                        arc.end = next_offset.start;
                        arc.center = element.end;
                        arc.anticlockwise = arc_anticlockwise;
                        
                        // Only add arc if it's not degenerate
                        if (!is_degenerate_element(arc)) {
                            temp_elements.push_back(arc);
                        }
                    }
                }
            }
        }
    }
    
    // If no valid elements, return original shape
    if (temp_elements.empty()) {
        return original_shape;
    }
    
    // Step 3: Handle self-intersections
    std::vector<ShapeElement> non_intersecting_elements = 
        remove_intersections_segments(temp_elements, is_deflating);
    
    // Build final shape
    inflated_shape.elements = non_intersecting_elements;
    
    return inflated_shape;
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
        
        
        // Calculate the original arc's start and end angles
        Angle start_angle = angle_radian(element.start - element.center);
        Angle end_angle = angle_radian(element.end - element.center);
        
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
        return equal(element.start, element.end);
    } else if (element.type == ShapeElementType::CircularArc) {
        // If the radius is very small, consider it degenerate
        if (equal(element.center, element.end)) {
            return true;
        }
        // If start and end points are too close, consider it degenerate
        if (equal(element.start, element.end)) {
            return true;
        }
    }
    
    return false;
}

}
}


/*
 * Minkowski sum implementation:
 * 1. First detect convex/concave vertices on the original shape
 * 2. Then offset each original element along its normal direction
 * 3. Based on inflation/deflation value:
 *    - For positive value (inflation): 
 *      - Add counterclockwise arcs at convex points
 *      - No special handling for concave points
 *    - For negative value (deflation):
 *      - Add clockwise arcs at concave points
 *      - No special handling for convex points
 * 4. Finally detect and remove self-intersections
 */
