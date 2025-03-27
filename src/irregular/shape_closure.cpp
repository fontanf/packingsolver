#include "irregular/shape_closure.hpp"
#include <cmath>

// Define M_PI if not provided by the system
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace packingsolver
{
namespace irregular
{

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
        LengthDbl gap_distance = distance(current.end, next.start);
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
                // For inflation, add a circular arc connector on corner points
                bool need_connector = true;
                if (current.end == next.start) {
                    // The end point of the current element and the start point of the next element coincide, no connector needed
                    need_connector = false;
                }
                
                if (need_connector) {
                    // Add a circular arc connector
                    ShapeElement connector;
                    connector.type = ShapeElementType::CircularArc;
                    connector.start = current.end;
                    connector.end = next.start;
                    
                    // std::cout << "\n==== Debug: Adding Arc Connector ====" << std::endl;
                    // std::cout << "Connection points: start(" << current.end.x << ", " << current.end.y 
                    //           << "), end(" << next.start.x << ", " << next.start.y << ")" << std::endl;
                    
                    // Try to find the original corner point (discontinuous slope point) as the center
                    // Point center_point;
                    // now to find the original corner point (discontinuous slope point) as the center
                    const ShapeElement& current_orig_elem = original_to_inflated_mapping[i].first;
                    // const ShapeElement& next_orig_elem = original_to_inflated_mapping[i+1].first;
                    connector.center = current_orig_elem.end;
                    connector.anticlockwise = true;
                    inflated_shape.elements.push_back(connector);
                }
            }
        }
    }
    
    // Remove duplicate points or degenerate elements
    if (inflated_shape.elements.size() > 1) {
        std::vector<ShapeElement> cleaned_elements;
        cleaned_elements.reserve(inflated_shape.elements.size());
        
        for (const ShapeElement& element : inflated_shape.elements) {
            if (!equal(distance(element.start, element.end), 0.0)) {
                cleaned_elements.push_back(element);
            }
        }
        
        inflated_shape.elements = cleaned_elements;
    }
    
    return inflated_shape;
}

}
}
