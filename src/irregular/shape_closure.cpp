#include "irregular/shape_closure.hpp"
#include <cmath>
#include <iostream>

// Define M_PI if not provided by the system
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace packingsolver
{
namespace irregular
{

LengthDbl euclidean_distance(const Point& p1, const Point& p2) {
    LengthDbl dx = p2.x - p1.x;
    LengthDbl dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool are_line_segments_collinear(
        const Point& line1_start, 
        const Point& line1_end, 
        const Point& line2_start, 
        const Point& line2_end) 
{
    // Calculate direction vectors
    LengthDbl dx1 = line1_end.x - line1_start.x;
    LengthDbl dy1 = line1_end.y - line1_start.y;
    LengthDbl length1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    
    LengthDbl dx2 = line2_end.x - line2_start.x;
    LengthDbl dy2 = line2_end.y - line2_start.y;
    LengthDbl length2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    
    if (length1 > 0 && length2 > 0) {
        // Calculate unit direction vectors
        LengthDbl dir1_x = dx1 / length1;
        LengthDbl dir1_y = dy1 / length1;
        LengthDbl dir2_x = dx2 / length2;
        LengthDbl dir2_y = dy2 / length2;
        
        // Calculate dot product to determine if they are parallel
        LengthDbl dot_product = dir1_x * dir2_x + dir1_y * dir2_y;
        
        // If dot product is close to ±1, vectors are parallel or antiparallel
        return equal(std::abs(dot_product), 1.0);
    }
    
    return false;
}

bool is_line_segment_tangent_to_arc(
        const Point& line_start, 
        const Point& line_end, 
        const Point& arc_center, 
        const Point& arc_start) 
{
    // Calculate line segment direction vector
    LengthDbl dx = line_end.x - line_start.x;
    LengthDbl dy = line_end.y - line_start.y;
    LengthDbl length = std::sqrt(dx * dx + dy * dy);
    
    // Calculate vector from center to arc start
    LengthDbl cx_to_start_x = arc_start.x - arc_center.x;
    LengthDbl cx_to_start_y = arc_start.y - arc_center.y;
    LengthDbl radius = std::sqrt(cx_to_start_x * cx_to_start_x + cx_to_start_y * cx_to_start_y);
    
    if (length > 0 && radius > 0) {
        // Arc normal vector at start point (unit vector from center to start)
        LengthDbl arc_normal_x = cx_to_start_x / radius;
        LengthDbl arc_normal_y = cx_to_start_y / radius;
        
        // Line segment direction vector
        LengthDbl line_dir_x = dx / length;
        LengthDbl line_dir_y = dy / length;
        
        // Calculate dot product to determine if they are perpendicular (i.e., line segment is parallel to arc tangent)
        LengthDbl dot_product = line_dir_x * arc_normal_x + line_dir_y * arc_normal_y;
        
        // If close to zero, the line segment is parallel to the arc tangent, i.e., tangent
        return equal(dot_product, 0.0);
    }
    
    return false;
}

bool are_arcs_tangent(const ShapeElement& arc1, const ShapeElement& arc2) 
{
    // Ensure both elements are arcs
    if (arc1.type != ShapeElementType::CircularArc || arc2.type != ShapeElementType::CircularArc) {
        return false;
    }
    
    // Calculate the tangent vector at the end point of the first arc
    LengthDbl arc1_end_angle = std::atan2(
        arc1.end.y - arc1.center.y,
        arc1.end.x - arc1.center.x
    );
    
    LengthDbl arc1_end_tangent_x, arc1_end_tangent_y;
    if (arc1.anticlockwise) {
        arc1_end_tangent_x = -std::sin(arc1_end_angle);
        arc1_end_tangent_y = std::cos(arc1_end_angle);
    } else {
        arc1_end_tangent_x = std::sin(arc1_end_angle);
        arc1_end_tangent_y = -std::cos(arc1_end_angle);
    }
    
    // Calculate the tangent vector at the start point of the second arc
    LengthDbl arc2_start_angle = std::atan2(
        arc2.start.y - arc2.center.y,
        arc2.start.x - arc2.center.x
    );
    
    LengthDbl arc2_start_tangent_x, arc2_start_tangent_y;
    if (arc2.anticlockwise) {
        arc2_start_tangent_x = -std::sin(arc2_start_angle);
        arc2_start_tangent_y = std::cos(arc2_start_angle);
    } else {
        arc2_start_tangent_x = std::sin(arc2_start_angle);
        arc2_start_tangent_y = -std::cos(arc2_start_angle);
    }
    
    // Calculate dot product to determine if the two tangent vectors are parallel or antiparallel
    LengthDbl dot_product = arc1_end_tangent_x * arc2_start_tangent_x + 
                          arc1_end_tangent_y * arc2_start_tangent_y;
    
    // If close to 1 or -1, the two arcs are tangent
    return equal(std::abs(dot_product), 1.0);
}

Shape close_inflated_elements(const std::vector<ShapeElement>& inflated_elements, bool is_deflating)
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
        LengthDbl gap_distance = euclidean_distance(current.end, next.start);
        //std::cout << "  Gap distance: " << gap_distance << std::endl;
        
        if (!equal(gap_distance, 0.0)) {
            // The end point of the current element and the start point of the next element don't coincide, need to add a connector
            //std::cout << "  Need connector (gap > 1e-6)" << std::endl;
            
            // For deflation, we handle connectors differently
            if (is_deflating) {
                // In deflation mode, simply add a straight line connector for all cases
                // This avoids adding circular arcs which could extend beyond the desired boundary
                ShapeElement connector;
                connector.type = ShapeElementType::LineSegment;
                connector.start = current.end;
                connector.end = next.start;
                inflated_shape.elements.push_back(connector);
                //std::cout << "  Deflation mode: adding straight line connector from ("
                //          << connector.start.x << "," << connector.start.y << ") to ("
                //          << connector.end.x << "," << connector.end.y << ")" << std::endl;
            } else {
                // For inflation, use the existing approach with curved connectors
                // Original deflation approach follows
                
                // Determine the connection type
                bool need_connector = true;
                
                if (current.type == ShapeElementType::LineSegment && next.type == ShapeElementType::LineSegment) {
                    // Line segment to line segment, check if collinear
                    if (are_line_segments_collinear(current.start, current.end, next.start, next.end)) {
                        // Collinear, add a straight line connector
                        //std::cout << "  Line segments are collinear, adding straight line connector" << std::endl;
                        ShapeElement connector;
                        connector.type = ShapeElementType::LineSegment;
                        connector.start = current.end;
                        connector.end = next.start;
                        inflated_shape.elements.push_back(connector);
                        need_connector = false;
                    } else {
                        //std::cout << "  Line segments are NOT collinear" << std::endl;
                    }
                } else if (current.type == ShapeElementType::LineSegment && next.type == ShapeElementType::CircularArc) {
                    // Line segment to circular arc, check if tangent
                    if (is_line_segment_tangent_to_arc(current.start, current.end, next.center, next.start)) {
                        // Tangent, no connector needed
                        need_connector = false;
                    }
                } else if (current.type == ShapeElementType::CircularArc && next.type == ShapeElementType::LineSegment) {
                    // Circular arc to line segment, check if tangent
                    if (is_line_segment_tangent_to_arc(next.start, next.end, current.center, current.end)) {
                        // Tangent, no connector needed
                        need_connector = false;
                    }
                } else if (current.type == ShapeElementType::CircularArc && next.type == ShapeElementType::CircularArc) {
                    // Circular arc to circular arc, check if tangent
                    if (are_arcs_tangent(current, next)) {
                        // Tangent, no connector needed
                        need_connector = false;
                    }
                }
                
                if (need_connector) {
                    // Add a circular arc connector
                    //std::cout << "  Using circular arc connector for larger gap (>= 0.1)" << std::endl;
                    ShapeElement connector;
                    connector.type = ShapeElementType::CircularArc;
                    connector.start = current.end;
                    connector.end = next.start;
                    
                    // Calculate the direction vector of the current element
                    LengthDbl current_dx = current.end.x - current.start.x;
                    LengthDbl current_dy = current.end.y - current.start.y;
                    if (current.type == ShapeElementType::CircularArc) {
                        // For arcs, need to calculate the tangent direction at the end point
                        LengthDbl end_angle = std::atan2(
                            current.end.y - current.center.y,
                            current.end.x - current.center.x
                        );
                        
                        if (current.anticlockwise) {
                            current_dx = -std::sin(end_angle);
                            current_dy = std::cos(end_angle);
                        } else {
                            current_dx = std::sin(end_angle);
                            current_dy = -std::cos(end_angle);
                        }
                    }
                    
                    // Calculate the direction vector of the next element
                    LengthDbl next_dx = next.end.x - next.start.x;
                    LengthDbl next_dy = next.end.y - next.start.y;
                    if (next.type == ShapeElementType::CircularArc) {
                        // For arcs, need to calculate the tangent direction at the start point
                        LengthDbl start_angle = std::atan2(
                            next.start.y - next.center.y,
                            next.start.x - next.center.x
                        );
                        
                        if (next.anticlockwise) {
                            next_dx = -std::sin(start_angle);
                            next_dy = std::cos(start_angle);
                        } else {
                            next_dx = std::sin(start_angle);
                            next_dy = -std::cos(start_angle);
                        }
                    }
                    
                    //std::cout << "  Current direction: (" << current_dx << "," << current_dy << ")" << std::endl;
                    //std::cout << "  Next direction: (" << next_dx << "," << next_dy << ")" << std::endl;
                    
                    // Normalize direction vectors
                    LengthDbl current_length = std::sqrt(current_dx * current_dx + current_dy * current_dy);
                    LengthDbl next_length = std::sqrt(next_dx * next_dx + next_dy * next_dy);
                    
                    if (current_length > 0 && next_length > 0) {
                        current_dx /= current_length;
                        current_dy /= current_length;
                        next_dx /= next_length;
                        next_dy /= next_length;
                        
                        //std::cout << "  Normalized current direction: (" << current_dx << "," << current_dy << ")" << std::endl;
                        //std::cout << "  Normalized next direction: (" << next_dx << "," << next_dy << ")" << std::endl;
                        
                        // Calculate the middle vector at the connection point
                        LengthDbl mid_dx = current_dx + next_dx;
                        LengthDbl mid_dy = current_dy + next_dy;
                        LengthDbl mid_length = std::sqrt(mid_dx * mid_dx + mid_dy * mid_dy);
                        
                        //std::cout << "  Middle vector: (" << mid_dx << "," << mid_dy << "), length: " << mid_length << std::endl;
                        
                        if (mid_length > 0.01) {  // Ensure the middle vector is valid
                            mid_dx /= mid_length;
                            mid_dy /= mid_length;
                            
                            // Calculate the normal vector to the middle vector
                            LengthDbl normal_dx = -mid_dy;
                            LengthDbl normal_dy = mid_dx;
                            
                            //std::cout << "  Normalized middle vector: (" << mid_dx << "," << mid_dy << ")" << std::endl;
                            //std::cout << "  Normal vector: (" << normal_dx << "," << normal_dy << ")" << std::endl;
                            
                            // Detect whether the shape is concave or convex at this point
                            // Calculate the turning from current direction to next direction
                            LengthDbl cross_product = current_dx * next_dy - current_dy * next_dx;
                            
                            // cross_product > 0 indicates counterclockwise turning (concave), < 0 indicates clockwise turning (convex)
                            bool is_concave = cross_product > 0;
                            
                            //std::cout << "  Cross product: " << cross_product << ", is_concave: " << (is_concave ? "true" : "false") << std::endl;
                            
                            // Calculate the chord length connecting the two points
                            LengthDbl dx = connector.end.x - connector.start.x;
                            LengthDbl dy = connector.end.y - connector.start.y;
                            LengthDbl chord_length = std::sqrt(dx * dx + dy * dy);
                            
                            // Calculate the midpoint of the connection
                            LengthDbl mid_x = (connector.start.x + connector.end.x) / 2;
                            LengthDbl mid_y = (connector.start.y + connector.end.y) / 2;
                            
                            //std::cout << "  Chord length: " << chord_length << std::endl;
                            //std::cout << "  Midpoint: (" << mid_x << "," << mid_y << ")" << std::endl;
                            
                            // Determine center position based on concavity/convexity (normal vector direction)
                            if (is_concave) {
                                // Concave point, center should be inside the shape, reverse the normal vector
                                normal_dx = -normal_dx;
                                normal_dy = -normal_dy;
                                connector.anticlockwise = false;  // Clockwise direction
                                //std::cout << "  Concave point, reversed normal: (" << normal_dx << "," << normal_dy << ")" << std::endl;
                            } else {
                                // Convex point, center should be outside the shape
                                connector.anticlockwise = true;   // Counterclockwise direction
                                //std::cout << "  Convex point, normal unchanged" << std::endl;
                            }
                            
                            // Calculate arc radius
                            // Use chord length and angle to determine an appropriate radius
                            LengthDbl angle_between = std::acos(std::max(-0.999, std::min(0.999, current_dx * next_dx + current_dy * next_dy)));
                            if (angle_between < 0.01) angle_between = 0.01;  // Avoid division by values close to zero
                            
                            //std::cout << "  Angle between directions: " << angle_between << " radians (" << (angle_between * 180.0 / M_PI) << " degrees)" << std::endl;
                            
                            LengthDbl radius = chord_length / (2 * std::sin(angle_between / 2));
                            
                            //std::cout << "  Initial radius calculation: " << radius << std::endl;
                            
                            // Adaptive radius limitation based on gap distance
                            LengthDbl min_radius = chord_length / 2;
                            LengthDbl max_radius = chord_length * 5;
                            
                            // Limit radius to a reasonable range
                            if (radius < min_radius) radius = min_radius;
                            if (radius > max_radius) radius = max_radius;
                            
                            //std::cout << "  Final radius after limits: " << radius << " (min: " << min_radius << ", max: " << max_radius << ")" << std::endl;
                            
                            // Calculate center
                            connector.center = Point{
                                mid_x + normal_dx * radius,
                                mid_y + normal_dy * radius
                            };
                            
                            //std::cout << "  Arc center: (" << connector.center.x << "," << connector.center.y << "), anticlockwise: " << (connector.anticlockwise ? "true" : "false") << std::endl;
                            
                            inflated_shape.elements.push_back(connector);
                        } else {
                            // Fallback to line segment if middle vector is invalid
                            ShapeElement line_connector;
                            line_connector.type = ShapeElementType::LineSegment;
                            line_connector.start = current.end;
                            line_connector.end = next.start;
                            inflated_shape.elements.push_back(line_connector);
                        }
                    } else {
                        // Fallback to line segment if direction calculation fails
                        ShapeElement line_connector;
                        line_connector.type = ShapeElementType::LineSegment;
                        line_connector.start = current.end;
                        line_connector.end = next.start;
                        inflated_shape.elements.push_back(line_connector);
                    }
                }
            }
        }
    }
    
    // Remove duplicate points or degenerate elements
    if (inflated_shape.elements.size() > 1) {
        std::vector<ShapeElement> cleaned_elements;
        cleaned_elements.reserve(inflated_shape.elements.size());
        
        for (const ShapeElement& element : inflated_shape.elements) {
            if (!equal(euclidean_distance(element.start, element.end), 0.0)) {
                cleaned_elements.push_back(element);
            }
        }
        
        inflated_shape.elements = cleaned_elements;
    }
    
    return inflated_shape;
}

}
}
