#include "irregular/shape_self_intersections_removal.hpp"

#include "irregular/shape_element_intersections.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

std::vector<ShapeElement> compute_splitted_elements(
        const std::vector<ShapeElement>& elements)
{
    //std::cout << "compute_splitted_elements" << std::endl;
    std::vector<std::vector<Point>> element_intersections(elements.size());
    for (ElementPos element_pos_1 = 0;
            element_pos_1 < (ElementPos)elements.size();
            ++element_pos_1) {
        const ShapeElement& element_1 = elements[element_pos_1];
        for (ElementPos element_pos_2 = element_pos_1 + 1;
                element_pos_2 < (ElementPos)elements.size();
                ++element_pos_2) {
            const ShapeElement& element_2 = elements[element_pos_2];
            auto intersection_points = compute_intersections(
                    element_1,
                    element_2);
            for (const Point& point: intersection_points) {
                //std::cout << "element_pos_1 " << element_pos_1
                //    << " " << element_1.to_string()
                //    << " element_pos_2 " << element_pos_2
                //    << " " << element_2.to_string()
                //    << " intersection " << point.to_string()
                //    << std::endl;
                element_intersections[element_pos_1].push_back(point);
                element_intersections[element_pos_2].push_back(point);
            }
        }
    }

    std::vector<ShapeElement> new_elements;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)elements.size();
            ++element_pos) {
        const ShapeElement& element = elements[element_pos];
        //std::cout << "element_pos " << element_pos
        //    << " " << element.to_string()
        //    << std::endl;
        // Sort intersection points of this element.
        std::sort(
                element_intersections[element_pos].begin(),
                element_intersections[element_pos].end(),
                [&element](
                    const Point& point_1,
                    const Point& point_2)
                {
                    return distance(element.start, point_1)
                        < distance(element.start, point_2);
                });
        // Create new elements.
        Point point_prev = element.start;
        for (const Point& point_cur: element_intersections[element_pos]) {
            // Skip segment ends and duplicated intersections.
            if (point_cur == element.start
                    || point_cur == element.end
                    || point_cur == point_prev) {
                continue;
            }
            ShapeElement new_element = element;
            new_element.start = point_prev;
            new_element.end = point_cur;
            new_elements.push_back(new_element);
            //std::cout << "- " << new_element.to_string() << std::endl;
            point_prev = point_cur;
        }
        ShapeElement new_element = element;
        new_element.start = point_prev;
        new_element.end = element.end;
        //std::cout << "- " << new_element.to_string() << std::endl;
        new_elements.push_back(new_element);
    }
    //std::cout << "compute_splitted_elements end" << std::endl;
    return new_elements;
}

struct ShapeSelfIntersectionRemovalArc
{
    NodeId source_node_id = -1;

    NodeId end_node_id = -1;

    ElementPos element_pos = -1;
};

struct ShapeSelfIntersectionRemovalNode
{
    std::vector<ElementPos> successors;
};

struct ShapeSelfIntersectionRemovalGraph
{
    std::vector<ShapeSelfIntersectionRemovalArc> arcs;

    std::vector<ShapeSelfIntersectionRemovalNode> nodes;
};

ShapeSelfIntersectionRemovalGraph compute_graph(
        const std::vector<ShapeElement>& new_elements)
{
    // Sort all element points.
    //std::cout << "compute_graph new_elements.size() " << new_elements.size() << std::endl;
    std::vector<std::pair<ElementPos, bool>> sorted_element_points;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)new_elements.size();
            ++element_pos) {
        sorted_element_points.push_back({element_pos, true});
        sorted_element_points.push_back({element_pos, false});
    }
    std::sort(
            sorted_element_points.begin(),
            sorted_element_points.end(),
            [&new_elements](
                const std::pair<ElementPos, bool>& p1,
                const std::pair<ElementPos, bool>& p2)
            {
                ElementPos element_pos_1 = p1.first;
                ElementPos element_pos_2 = p2.first;
                const ShapeElement& element_1 = new_elements[element_pos_1];
                const ShapeElement& element_2 = new_elements[element_pos_2];
                bool start_1 = p1.second;
                bool start_2 = p2.second;
                const Point& point_1 = (start_1)? element_1.start: element_1.end;
                const Point& point_2 = (start_2)? element_2.start: element_2.end;
                if (point_1.x != point_2.x)
                    return point_1.x < point_2.x;
                return point_1.y < point_2.y;
            });

    ShapeSelfIntersectionRemovalGraph graph;
    graph.arcs = std::vector<ShapeSelfIntersectionRemovalArc>(new_elements.size());
    // For each point associate a node id and build the graph edges.
    Point point_prev = {0, 0};
    for (ElementPos pos = 0; pos < (ElementPos)sorted_element_points.size(); ++pos) {
        ElementPos element_pos = sorted_element_points[pos].first;
        const ShapeElement& element = new_elements[element_pos];
        bool start = sorted_element_points[pos].second;
        const Point& point = (start)? element.start: element.end;
        //std::cout << "pos " << pos
        //    << " element_pos " << element_pos
        //    << " start " << start
        //    << " point " << point.to_string()
        //    << std::endl;
        if (pos == 0 || !(point_prev == point))
            graph.nodes.push_back(ShapeSelfIntersectionRemovalNode());
        NodeId node_id = graph.nodes.size() - 1;
        if (start) {
            graph.arcs[element_pos].source_node_id = node_id;
            graph.nodes[node_id].successors.push_back(element_pos);
        } else {
            graph.arcs[element_pos].end_node_id = node_id;
        }
        point_prev = point;
    }
    //std::cout << "compute_graph end" << std::endl;
    return graph;
}

}

std::pair<Shape, std::vector<Shape>> packingsolver::irregular::remove_self_intersections(
        const Shape& shape)
{
    //std::cout << "remove_self_intersections shape " << shape.to_string(0) << std::endl;

    // Build directed graph.
    std::vector<ShapeElement> new_elements = compute_splitted_elements(shape.elements);
    ShapeSelfIntersectionRemovalGraph graph = compute_graph(new_elements);

    // Find most bottom-left element.
    ElementPos element_start_pos = -1;
    auto cmp = [&new_elements](
            ElementPos element_pos_1,
            ElementPos element_pos_2)
    {
        const ShapeElement& element_1 = new_elements[element_pos_1];
        const ShapeElement& element_2 = new_elements[element_pos_2];
        if (element_1.start.x != element_2.start.x)
            return element_1.start.x < element_2.start.x;
        if (element_1.start.y != element_2.start.y)
            return element_1.start.y < element_2.start.y;
        if (element_1.end.x != element_2.end.x)
            return element_1.end.x < element_2.end.x;
        return element_1.end.y < element_2.end.y;
    };
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)new_elements.size();
            ++element_pos) {
        const ShapeElement& element = new_elements[element_pos];
        if (element_start_pos == -1
                || cmp(element_pos, element_start_pos)) {
            element_start_pos = element_pos;
        }
    }

    std::vector<uint8_t> element_is_processed(new_elements.size(), 0);

    // Find outer loop.
    //std::cout << "find outer loop..." << std::endl;
    Shape new_shape;
    ElementPos element_cur_pos = element_start_pos;
    for (;;) {
        const ShapeElement& element_cur = new_elements[element_cur_pos];
        new_shape.elements.push_back(element_cur);
        element_is_processed[element_cur_pos] = 1;

        // Find the next element with the smallest angle.
        const ShapeSelfIntersectionRemovalArc& arc = graph.arcs[element_cur_pos];
        //std::cout
        //    << "element_cur_pos " << element_cur_pos
        //    << " " << element_cur.to_string()
        //    << " node_id " << arc.end_node_id << " / " << graph.nodes.size()
        //    << std::endl;
        const ShapeSelfIntersectionRemovalNode& node = graph.nodes[arc.end_node_id];
        ElementPos smallest_angle_element_pos = -1;
        Angle smallest_angle = 0.0;
        for (ElementPos element_pos_next: node.successors) {
            const ShapeElement& element_next = new_elements[element_pos_next];
            Angle angle = angle_radian(
                    element_cur.start - element_cur.end,
                    element_next.end - element_next.start);
            //std::cout << "- element_pos_next " << element_pos_next
            //    << " " << element_next.to_string()
            //    << " angle " << angle
            //    << std::endl;
            if (smallest_angle_element_pos == -1
                    || smallest_angle > angle) {
                smallest_angle_element_pos = element_pos_next;
                smallest_angle = angle;
            }
        }

        // Update current element.
        element_cur_pos = smallest_angle_element_pos;
        if (element_cur_pos == element_start_pos)
            break;
    }
    new_shape = clean_shape(new_shape);

    // Find holes by finding paths from remaining unprocessed vertices.
    std::vector<Shape> new_holes;
    for (;;) {
        // Find an unprocessed element.
        ElementPos element_start_pos = -1;
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)new_elements.size();
                ++element_pos) {
            const ShapeElement& element = new_elements[element_pos];
            if (!element_is_processed[element_pos]) {
                element_start_pos = element_pos;
                break;
            }
        }
        // If all elements have already been processed, stop.
        if (element_start_pos == -1)
            break;

        //std::cout << "find new hole..." << std::endl;
        Shape new_hole;
        ElementPos element_cur_pos = element_start_pos;
        for (;;) {
            const ShapeElement& element_cur = new_elements[element_cur_pos];
            new_hole.elements.push_back(element_cur);
            element_is_processed[element_cur_pos] = 1;

            // Find the next element with the smallest angle.
            const ShapeSelfIntersectionRemovalArc& arc = graph.arcs[element_cur_pos];
            //std::cout
            //    << "element_cur_pos " << element_cur_pos
            //    << " " << element_cur.to_string()
            //    << " node_id " << arc.end_node_id << " / " << graph.nodes.size()
            //    << std::endl;
            const ShapeSelfIntersectionRemovalNode& node = graph.nodes[arc.end_node_id];
            ElementPos smallest_angle_element_pos = -1;
            Angle smallest_angle = 0.0;
            for (ElementPos element_pos_next: node.successors) {
                const ShapeElement& element_next = new_elements[element_pos_next];
                Angle angle = angle_radian(
                        element_cur.start - element_cur.end,
                        element_next.end - element_next.start);
                //std::cout << "* element_pos_next " << element_pos_next
                //    << " " << element_next.to_string()
                //    << " angle " << angle
                //    << std::endl;
                if (smallest_angle_element_pos == -1
                        || smallest_angle > angle) {
                    smallest_angle_element_pos = element_pos_next;
                    smallest_angle = angle;
                }
            }

            // Update current element.
            element_cur_pos = smallest_angle_element_pos;

            // Check if hole is finished.
            if (element_is_processed[element_cur_pos]) {
                ElementPos pos_best = -1;
                for (ElementPos pos = 0; pos < (ElementPos)new_hole.elements.size(); ++pos) {
                    if (new_hole.elements[pos] == new_elements[element_cur_pos]) {
                        pos_best = pos;
                        break;
                    }
                }
                //std::cout << "pos_best " << pos_best << std::endl;
                if (pos_best != -1) {
                    Shape new_hole_2;
                    for (ElementPos pos = pos_best;
                            pos < (ElementPos)new_hole.elements.size();
                            ++pos) {
                        new_hole_2.elements.push_back(new_hole.elements[pos]);
                    }
                    new_hole_2 = new_hole_2.reverse();
                    if (new_hole_2.compute_area() > 0) {
                        new_hole_2 = clean_shape(new_hole_2);
                        new_holes.push_back(new_hole_2);
                    }
                }
                break;
            }
        }
    }

    //std::cout << "remove_self_intersections end" << std::endl;
    return {new_shape, new_holes};
}

std::vector<Shape> irregular::extract_all_holes_from_self_intersecting_hole(
        const Shape& shape)
{
    //std::cout << "extract_all_holes_from_self_intersecting_shape shape " << shape.to_string(0) << std::endl;;

    // Build directed graph.
    std::vector<ShapeElement> new_elements = compute_splitted_elements(shape.elements);
    ShapeSelfIntersectionRemovalGraph graph = compute_graph(new_elements);

    std::vector<uint8_t> element_is_processed(new_elements.size(), 0);

    // Find holes by finding paths from remaining unprocessed vertices.
    std::vector<Shape> new_holes;
    for (;;) {
        // Find an unprocessed element.
        ElementPos element_start_pos = -1;
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)new_elements.size();
                ++element_pos) {
            const ShapeElement& element = new_elements[element_pos];
            if (!element_is_processed[element_pos]) {
                element_start_pos = element_pos;
                break;
            }
        }
        // If all elements have already been processed, stop.
        if (element_start_pos == -1)
            break;

        Shape new_hole;
        ElementPos element_cur_pos = element_start_pos;
        for (;;) {
            const ShapeElement& element_cur = new_elements[element_cur_pos];
            new_hole.elements.push_back(element_cur);
            element_is_processed[element_cur_pos] = 1;

            // Find the next element with the largest angle.
            const ShapeSelfIntersectionRemovalArc& arc = graph.arcs[element_cur_pos];
            //std::cout
            //    << "element_cur_pos " << element_cur_pos
            //    << " " << element_cur.to_string()
            //    << " node_id " << arc.end_node_id << " / " << graph.nodes.size()
            //    << std::endl;
            const ShapeSelfIntersectionRemovalNode& node = graph.nodes[arc.end_node_id];
            ElementPos largest_angle_element_pos = -1;
            Angle largest_angle = 0.0;
            for (ElementPos element_pos_next: node.successors) {
                const ShapeElement& element_next = new_elements[element_pos_next];
                Angle angle = angle_radian(
                        element_cur.start - element_cur.end,
                        element_next.end - element_next.start);
                if (largest_angle_element_pos == -1
                        || largest_angle < angle) {
                    largest_angle_element_pos = element_pos_next;
                    largest_angle = angle;
                }
            }
            if (largest_angle_element_pos == -1)
                break;

            // Update current element.
            element_cur_pos = largest_angle_element_pos;

            // Check if hole is finished.
            if (element_is_processed[element_cur_pos]) {
                ElementPos pos_best = -1;
                for (ElementPos pos = 0; pos < (ElementPos)new_hole.elements.size(); ++pos) {
                    if (new_hole.elements[pos] == new_elements[element_cur_pos]) {
                        pos_best = pos;
                        break;
                    }
                }
                //std::cout << "pos_best " << pos_best << std::endl;
                if (pos_best != -1) {
                    Shape new_hole_2;
                    for (ElementPos pos = pos_best;
                            pos < (ElementPos)new_hole.elements.size();
                            ++pos) {
                        new_hole_2.elements.push_back(new_hole.elements[pos]);
                    }
                    //std::cout << "area " << new_hole_2.compute_area() << std::endl;
                    if (new_hole_2.compute_area() > 0) {
                        new_hole_2 = clean_shape(new_hole_2);
                        new_holes.push_back(new_hole_2);
                    }
                }
                break;
            }
        }
    }

    //std::cout << "extract_all_holes_from_self_intersecting_shape end" << std::endl;
    return new_holes;
}

std::vector<ShapeElement> packingsolver::irregular::remove_intersections_segments(
        const std::vector<ShapeElement>& elements,
        bool is_deflating)
{
    if (elements.empty()) {
        return elements;
    }
    
    //std::cout << "\n------ Debug: remove_intersections_segments ------" << std::endl;
    //std::cout << "Input elements: " << elements.size() << std::endl;
    //std::cout << "Is deflating: " << (is_deflating ? "true" : "false") << std::endl;
    
    // Copy input elements for processing
    std::vector<ShapeElement> working_elements = elements;
    
    // Extend line segments to ensure intersection
    for (size_t i = 0; i < working_elements.size(); ++i) {
        if (working_elements[i].type != ShapeElementType::LineSegment) {
            continue;
        }
        
        LengthDbl dx = working_elements[i].end.x - working_elements[i].start.x;
        LengthDbl dy = working_elements[i].end.y - working_elements[i].start.y;
        // LengthDbl length = std::sqrt(dx*dx + dy*dy);
        
        if (!equal(working_elements[i].start, working_elements[i].end)) {
            // Extend by 10%
            LengthDbl extension_ratio = 0.1;
            Point original_end = working_elements[i].end;
            
            working_elements[i].end.x = original_end.x + dx * extension_ratio;
            working_elements[i].end.y = original_end.y + dy * extension_ratio;
            
            //std::cout << "Extended element " << i << " from ("
            //        << original_end.x << "," << original_end.y << ") to ("
            //        << working_elements[i].end.x << "," << working_elements[i].end.y << ")" << std::endl;
        }
    }
    
    // Find all intersections
    std::vector<std::vector<Point>> intersections(working_elements.size());
    
    for (size_t i = 0; i < working_elements.size(); ++i) {
        for (size_t j = i + 1; j < working_elements.size(); ++j) {
            std::vector<Point> points = compute_intersections(working_elements[i], working_elements[j]);
            
            for (size_t k = 0; k < points.size(); ++k) {
                intersections[i].push_back(points[k]);
                intersections[j].push_back(points[k]);
                //std::cout << "Found intersection between elements " << i << " and " << j
                //        << " at (" << points[k].x << "," << points[k].y << ")" << std::endl;
            }
        }
    }
    
    // Find the bounding box of all intersection points
    Point min_point = {std::numeric_limits<LengthDbl>::max(), std::numeric_limits<LengthDbl>::max()};
    Point max_point = {std::numeric_limits<LengthDbl>::lowest(), std::numeric_limits<LengthDbl>::lowest()};
    
    bool has_intersections = false;
    for (size_t i = 0; i < intersections.size(); ++i) {
        for (size_t j = 0; j < intersections[i].size(); ++j) {
            const Point& p = intersections[i][j];
            min_point.x = std::min(min_point.x, p.x);
            min_point.y = std::min(min_point.y, p.y);
            max_point.x = std::max(max_point.x, p.x);
            max_point.y = std::max(max_point.y, p.y);
            has_intersections = true;
        }
    }
    
    if (has_intersections) {
        //std::cout << "Intersection bounding box: (" << min_point.x << "," << min_point.y
        //        << ") to (" << max_point.x << "," << max_point.y << ")" << std::endl;
    }
    
    // Process results
    std::vector<ShapeElement> result;
    
    for (size_t i = 0; i < elements.size(); ++i) {
        const ShapeElement& element = elements[i];
        
        // Skip degenerate elements
        if (equal(element.start, element.end)) {
            continue;
        }
        
        // Get intersections for this element
        std::vector<Point> element_intersections = intersections[i];
        
        // If no intersections, handle based on deflation mode
        if (element_intersections.empty()) {
            if (is_deflating) {
                // In deflation mode, need to check if this element should be included
                bool is_inside_bbox = is_point_inside_box(element.start, min_point, max_point) &&
                                     is_point_inside_box(element.end, min_point, max_point);
                if (is_inside_bbox) {
                    result.push_back(element);
                    //std::cout << "Kept element with no intersections (inside bbox)" << std::endl;
                } else {
                    //std::cout << "Skipped element with no intersections (outside bbox)" << std::endl;
                }
            } else {
                // For inflation, keep all elements
                result.push_back(element);
            }
            continue;
        }
        
        // Sort intersections by distance from start
        std::sort(element_intersections.begin(), element_intersections.end(),
            [&element](const Point& p1, const Point& p2) {
                LengthDbl d1 = std::sqrt(
                    std::pow(p1.x - element.start.x, 2) +
                    std::pow(p1.y - element.start.y, 2));
                LengthDbl d2 = std::sqrt(
                    std::pow(p2.x - element.start.x, 2) +
                    std::pow(p2.y - element.start.y, 2));
                return d1 < d2;
            });
        
        // Remove endpoints
        std::vector<Point> filtered_intersections;
        for (size_t j = 0; j < element_intersections.size(); ++j) {
            const Point& p = element_intersections[j];
            if (!equal(p, element.start) && !equal(p, element.end)) {
                filtered_intersections.push_back(p);
            }
        }
        
        // Remove close points
        std::vector<Point> final_intersections;
        for (size_t j = 0; j < filtered_intersections.size(); ++j) {
            bool should_add = true;
            for (size_t k = 0; k < final_intersections.size(); ++k) {
                if (equal(filtered_intersections[j], final_intersections[k])) {
                    should_add = false;
                    break;
                }
            }
            if (should_add) {
                final_intersections.push_back(filtered_intersections[j]);
            }
        }
        
        // If no intersections after filtering, handle same as earlier
        if (final_intersections.empty()) {
            if (is_deflating) {
                // In deflation mode, need to check if this element should be included
                bool is_inside_bbox = is_point_inside_box(element.start, min_point, max_point) &&
                                     is_point_inside_box(element.end, min_point, max_point);
                if (is_inside_bbox) {
                    result.push_back(element);
                    //std::cout << "Kept element with no valid intersections (inside bbox)" << std::endl;
                } else {
                    //std::cout << "Skipped element with no valid intersections (outside bbox)" << std::endl;
                }
            } else {
                // For inflation, keep all elements
                result.push_back(element);
            }
            continue;
        }
        
        //std::cout << "Element " << i << " has " << final_intersections.size() << " valid intersection points" << std::endl;
        
        // Generate sub-segments
        Point prev_point = element.start;
        for (size_t j = 0; j < final_intersections.size(); ++j) {
            const Point& current_point = final_intersections[j];
            
            // Check if point is within original element
            LengthDbl original_length = std::sqrt(
                std::pow(element.end.x - element.start.x, 2) +
                std::pow(element.end.y - element.start.y, 2));
            LengthDbl point_dist = std::sqrt(
                std::pow(current_point.x - element.start.x, 2) +
                std::pow(current_point.y - element.start.y, 2));
            
            if (point_dist > original_length) {
                //std::cout << "Skipping intersection point beyond original element end" << std::endl;
                continue;
            }
            
            // Create new segment
            ShapeElement new_segment = element;
            new_segment.start = prev_point;
            new_segment.end = current_point;
            
            // Add only non-degenerate segments
            if (!equal(new_segment.start, new_segment.end)) {
                if (is_deflating) {
                    // For deflation, check if segment is inside the bounding box
                    // For partial segments, only keep the part inside the box
                    bool start_inside = is_point_inside_box(new_segment.start, min_point, max_point);
                    bool end_inside = is_point_inside_box(new_segment.end, min_point, max_point);
                    
                    if (start_inside && end_inside) {
                        // Segment is fully inside, keep it
                        result.push_back(new_segment);
                        //std::cout << "Added segment (fully inside bbox) from (" 
                        //        << new_segment.start.x << "," << new_segment.start.y
                        //        << ") to (" << new_segment.end.x << "," << new_segment.end.y << ")" << std::endl;
                    }
                    // Note: For deflation, we skip segments that are partially or fully outside
                } else {
                    // For inflation, keep all segments
                    result.push_back(new_segment);
                    //std::cout << "Added segment from (" << new_segment.start.x << "," << new_segment.start.y
                    //        << ") to (" << new_segment.end.x << "," << new_segment.end.y << ")" << std::endl;
                }
            }
            
            prev_point = current_point;
        }
        
        // Add final segment
        ShapeElement last_segment = element;
        last_segment.start = prev_point;
        last_segment.end = element.end;
        
        // Add only non-degenerate segments
        if (!equal(last_segment.start, last_segment.end)) {
            if (is_deflating) {
                // For deflation, check if segment is inside the bounding box
                bool start_inside = is_point_inside_box(last_segment.start, min_point, max_point);
                bool end_inside = is_point_inside_box(last_segment.end, min_point, max_point);
                
                if (start_inside && end_inside) {
                    // Segment is fully inside, keep it
                    result.push_back(last_segment);
                    //std::cout << "Added final segment (fully inside bbox) from (" 
                    //        << last_segment.start.x << "," << last_segment.start.y
                    //        << ") to (" << last_segment.end.x << "," << last_segment.end.y << ")" << std::endl;
                }
                // Skip segments that are partially or fully outside
            } else {
                // For inflation, keep all segments
                result.push_back(last_segment);
                //std::cout << "Added final segment from (" << last_segment.start.x << "," << last_segment.start.y
                //        << ") to (" << last_segment.end.x << "," << last_segment.end.y << ")" << std::endl;
            }
        }
    }
    
    //std::cout << "Final output elements: " << result.size() << std::endl;
    return result;
}

// Helper function to check if a point is inside a box
bool packingsolver::irregular::is_point_inside_box(const Point& p, const Point& min_corner, const Point& max_corner) {
    return p.x >= min_corner.x && p.x <= max_corner.x && 
           p.y >= min_corner.y && p.y <= max_corner.y;
}
