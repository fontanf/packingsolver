#include "irregular/shape_self_intersections_removal.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{

// Helper function to check if a point lies on an arc
bool is_point_on_arc(const Point& p, const ShapeElement& arc)
{
    // Check if point lies on circle
    if (!equal(distance(p, arc.center), distance(arc.start, arc.center))) {
        return false;
    }
    
    // Calculate angles
    Angle point_angle = angle_radian(p - arc.center);
    Angle start_angle = angle_radian(arc.start - arc.center);
    Angle end_angle = angle_radian(arc.end - arc.center);
    
    // Normalize angles
    if (arc.anticlockwise) {
        if (end_angle <= start_angle) {
            end_angle += 2 * M_PI;
        }
        if (point_angle < start_angle) {
            point_angle += 2 * M_PI;
        }
        return point_angle >= start_angle && point_angle <= end_angle;
    } else {
        if (start_angle <= end_angle) {
            start_angle += 2 * M_PI;
        }
        if (point_angle < end_angle) {
            point_angle += 2 * M_PI;
        }
        return point_angle >= end_angle && point_angle <= start_angle;
    }
}

// Helper function to compute line-line intersections
std::vector<Point> compute_line_line_intersections(
        const ShapeElement& line1,
        const ShapeElement& line2)
{
    LengthDbl x1 = line1.start.x;
    LengthDbl y1 = line1.start.y;
    LengthDbl x2 = line1.end.x;
    LengthDbl y2 = line1.end.y;
    LengthDbl x3 = line2.start.x;
    LengthDbl y3 = line2.start.y;
    LengthDbl x4 = line2.end.x;
    LengthDbl y4 = line2.end.y;

    // Check if both line segments are colinear.
    LengthDbl denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (equal(denom, 0.0)) {
        // If they are colinear, check if they are aligned.
        LengthDbl denom_2 = (x1 - x2) * (y3 - y1) - (y1 - y2) * (x3 - x1);
        if (!equal(denom_2, 0.0))
            return {};

        // If they are aligned, check if they overlap.
        std::array<ElementPos, 4> sorted_points = {0, 1, 2, 3};
        std::sort(
                sorted_points.begin(),
                sorted_points.end(),
                [&line1, &line2](
                    ElementPos point_pos_1,
                    ElementPos point_pos_2)
                {
                    const Point& point_1 =
                        (point_pos_1 == 0)? line1.start:
                        (point_pos_1 == 1)? line1.end:
                        (point_pos_1 == 2)? line2.start:
                        line2.end;
                    const Point& point_2 =
                        (point_pos_2 == 0)? line1.start:
                        (point_pos_2 == 1)? line1.end:
                        (point_pos_2 == 2)? line2.start:
                        line2.end;
                    if (point_1.x != point_2.x)
                        return point_1.x < point_2.x;
                    return point_1.y < point_2.y;
                });
        if (sorted_points[0] + sorted_points[1] == 1
                || sorted_points[0] + sorted_points[1] == 5) {
            return {};
        }

        // Return the two interior points.
        const Point& point_1 =
            (sorted_points[1] == 0)? line1.start:
            (sorted_points[1] == 1)? line1.end:
            (sorted_points[1] == 2)? line2.start:
            line2.end;
        const Point& point_2 =
            (sorted_points[2] == 0)? line1.start:
            (sorted_points[2] == 1)? line1.end:
            (sorted_points[2] == 2)? line2.start:
            line2.end;
        return {point_1, point_2};
    }

    // Otherwise, compute intersection.
    LengthDbl t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    LengthDbl u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

    if (t < 0 || t > 1 || u < 0 || u > 1) {
        // No intersection.
        return {};
    } else if (equal(t, 0.0)) {
        return {line1.start};
    } else if (equal(t, 1.0)) {
        return {line1.end};
    } else if (equal(u, 0.0)) {
        return {line2.start};
    } else if (equal(u, 1.0)) {
        return {line2.end};
    } else {
        // Standard intersection.
        LengthDbl xp = x1 + t * (x2 - x1);
        LengthDbl yp = y1 + t * (y2 - y1);
        return {{xp, yp}};
    }
}

// Helper function to compute line-arc intersections
std::vector<Point> compute_line_arc_intersections(
        const ShapeElement& line,
        const ShapeElement& arc)
{
    // Transform line to quadratic equation coefficients
    LengthDbl dx = line.end.x - line.start.x;
    LengthDbl dy = line.end.y - line.start.y;
    LengthDbl length = distance(line.start, line.end);
    
    if (equal(length, 0.0)) return {};
    
    // Normalize direction vector
    dx /= length;
    dy /= length;
    
    // Line equation: P = P0 + t*d, where P0 is start point, d is direction
    // Circle equation: (x - cx)² + (y - cy)² = r²
    // Substituting line into circle gives quadratic equation
    LengthDbl cx = arc.center.x;
    LengthDbl cy = arc.center.y;
    LengthDbl r = distance(arc.center, arc.start);
    
    // Calculate quadratic equation coefficients
    LengthDbl px = line.start.x - cx;
    LengthDbl py = line.start.y - cy;
    
    LengthDbl a = dx * dx + dy * dy;  // Should be 1.0 due to normalization
    LengthDbl b = 2.0 * (px * dx + py * dy);
    LengthDbl c = px * px + py * py - r * r;
    
    // Solve quadratic equation
    LengthDbl discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return {};
    
    std::vector<Point> intersections;
    if (equal(discriminant, 0.0)) {
        // One intersection
        LengthDbl t = -b / (2 * a);
        if (t < 0 || t > length) return {};
        
        // Check if intersection is at endpoint of line segment
        if (equal(t, 0.0)) {
            // Intersection is at start point of line
            if (is_point_on_arc(line.start, arc)) {
                return {line.start};
            }
            return {};
        }
        
        if (equal(t, length)) {
            // Intersection is at end point of line
            if (is_point_on_arc(line.end, arc)) {
                return {line.end};
            }
            return {};
        }
        
        // Standard intersection point
        Point p = {
            line.start.x + t * dx,
            line.start.y + t * dy
        };
        if (is_point_on_arc(p, arc)) {
            intersections.push_back(p);
        }
    } else {
        // Two intersections
        LengthDbl t1 = (-b + std::sqrt(discriminant)) / (2 * a);
        LengthDbl t2 = (-b - std::sqrt(discriminant)) / (2 * a);
        
        // Check first intersection
        if (t1 >= 0 && t1 <= length) {
            if (equal(t1, 0.0)) {
                // Intersection at start point
                if (is_point_on_arc(line.start, arc)) {
                    intersections.push_back(line.start);
                }
            } else if (equal(t1, length)) {
                // Intersection at end point
                if (is_point_on_arc(line.end, arc)) {
                    intersections.push_back(line.end);
                }
            } else {
                // Standard intersection
                Point p1 = {
                    line.start.x + t1 * dx,
                    line.start.y + t1 * dy
                };
                if (is_point_on_arc(p1, arc)) {
                    intersections.push_back(p1);
                }
            }
        }
        
        // Check second intersection
        if (t2 >= 0 && t2 <= length) {
            if (equal(t2, 0.0)) {
                // Intersection at start point
                if (is_point_on_arc(line.start, arc) && 
                    (intersections.empty() || !equal(intersections[0].x, line.start.x) || !equal(intersections[0].y, line.start.y))) {
                    intersections.push_back(line.start);
                }
            } else if (equal(t2, length)) {
                // Intersection at end point
                if (is_point_on_arc(line.end, arc) && 
                    (intersections.empty() || !equal(intersections[0].x, line.end.x) || !equal(intersections[0].y, line.end.y))) {
                    intersections.push_back(line.end);
                }
            } else {
                // Standard intersection
                Point p2 = {
                    line.start.x + t2 * dx,
                    line.start.y + t2 * dy
                };
                if (is_point_on_arc(p2, arc)) {
                    // Ensure we're not adding a duplicate point
                    bool is_duplicate = false;
                    for (const auto& p : intersections) {
                        if (equal(p.x, p2.x) && equal(p.y, p2.y)) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (!is_duplicate) {
                        intersections.push_back(p2);
                    }
                }
            }
        }
    }
    
    // Check if any intersection coincides with an arc endpoint
    for (size_t i = 0; i < intersections.size(); ++i) {
        if (equal(intersections[i].x, arc.start.x) && equal(intersections[i].y, arc.start.y)) {
            // Replace with exact endpoint for numerical stability
            intersections[i] = arc.start;
        } else if (equal(intersections[i].x, arc.end.x) && equal(intersections[i].y, arc.end.y)) {
            // Replace with exact endpoint for numerical stability
            intersections[i] = arc.end;
        }
    }
    
    return intersections;
}

// Helper function to compute arc-arc intersections
std::vector<Point> compute_arc_arc_intersections(
        const ShapeElement& arc1,
        const ShapeElement& arc2)
{
    // Get circle centers and radii
    LengthDbl x1 = arc1.center.x;
    LengthDbl y1 = arc1.center.y;
    LengthDbl r1 = distance(arc1.center, arc1.start);
    
    LengthDbl x2 = arc2.center.x;
    LengthDbl y2 = arc2.center.y;
    LengthDbl r2 = distance(arc2.center, arc2.start);
    
    // Calculate distance between centers
    LengthDbl d = distance(arc1.center, arc2.center);
    
    // Check if circles are too far apart or one inside another
    if (d > r1 + r2 || d < std::abs(r1 - r2) || equal(d, 0.0)) {
        return {};
    }
    
    // Special case: circles touch externally or internally
    if (equal(d, r1 + r2) || equal(d, std::abs(r1 - r2))) {
        // Circles touch at exactly one point
        Point touching_point;
        if (equal(d, r1 + r2)) {
            // External touching
            touching_point = {
                x1 + r1 * (x2 - x1) / d,
                y1 + r1 * (y2 - y1) / d
            };
        } else {
            // Internal touching
            touching_point = {
                x1 + r1 * (x2 - x1) / d,
                y1 + r1 * (y2 - y1) / d
            };
        }
        
        // Check if the touching point lies on both arcs
        if (is_point_on_arc(touching_point, arc1) && is_point_on_arc(touching_point, arc2)) {
            return {touching_point};
        }
        return {};
    }
    
    // Calculate intersection points of circles
    LengthDbl a = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
    LengthDbl h = std::sqrt(r1 * r1 - a * a);
    
    // Calculate the point P2 that lies on the line between the centers
    LengthDbl x3 = x1 + a * (x2 - x1) / d;
    LengthDbl y3 = y1 + a * (y2 - y1) / d;
    
    // Calculate the intersection points
    std::vector<Point> intersections;
    
    // First intersection point
    Point p1 = {
        x3 + h * (y2 - y1) / d,
        y3 - h * (x2 - x1) / d
    };
    
    // Second intersection point
    Point p2 = {
        x3 - h * (y2 - y1) / d,
        y3 + h * (x2 - x1) / d
    };
    
    // Check if points lie on both arcs
    if (is_point_on_arc(p1, arc1) && is_point_on_arc(p1, arc2)) {
        // Check if p1 coincides with arc endpoints
        if (equal(distance(p1, arc1.start), 0.0)) {
            intersections.push_back(arc1.start);
        } else if (equal(distance(p1, arc1.end), 0.0)) {
            intersections.push_back(arc1.end);
        } else if (equal(distance(p1, arc2.start), 0.0)) {
            intersections.push_back(arc2.start);
        } else if (equal(distance(p1, arc2.end), 0.0)) {
            intersections.push_back(arc2.end);
        } else {
            intersections.push_back(p1);
        }
    }
    
    if (is_point_on_arc(p2, arc1) && is_point_on_arc(p2, arc2)) {
        // Check if p2 coincides with arc endpoints
        if (equal(distance(p2, arc1.start), 0.0)) {
            if (intersections.empty() || !equal(distance(intersections[0], arc1.start), 0.0)) {
                intersections.push_back(arc1.start);
            }
        } else if (equal(distance(p2, arc1.end), 0.0)) {
            if (intersections.empty() || !equal(distance(intersections[0], arc1.end), 0.0)) {
                intersections.push_back(arc1.end);
            }
        } else if (equal(distance(p2, arc2.start), 0.0)) {
            if (intersections.empty() || !equal(distance(intersections[0], arc2.start), 0.0)) {
                intersections.push_back(arc2.start);
            }
        } else if (equal(distance(p2, arc2.end), 0.0)) {
            if (intersections.empty() || !equal(distance(intersections[0], arc2.end), 0.0)) {
                intersections.push_back(arc2.end);
            }
        } else {
            // Check if p2 is duplicate of p1 (can happen due to floating point errors)
            if (intersections.empty() || !equal(distance(p2, intersections[0]), 0.0)) {
                intersections.push_back(p2);
            }
        }
    }
    
    return intersections;
}


std::vector<Point> compute_intersections(
        const ShapeElement& element_1,
        const ShapeElement& element_2)
{
    // Line segment - Line segment intersection
    if (element_1.type == ShapeElementType::LineSegment
            && element_2.type == ShapeElementType::LineSegment) {
        return compute_line_line_intersections(element_1, element_2);
    }
    // Line segment - Circular arc intersection
    else if (element_1.type == ShapeElementType::LineSegment
            && element_2.type == ShapeElementType::CircularArc) {
        return compute_line_arc_intersections(element_1, element_2);
    }
    else if (element_1.type == ShapeElementType::CircularArc
            && element_2.type == ShapeElementType::LineSegment) {
        return compute_line_arc_intersections(element_2, element_1);
    }
    // Circular arc - Circular arc intersection
    else if (element_1.type == ShapeElementType::CircularArc
            && element_2.type == ShapeElementType::CircularArc) {
        return compute_arc_arc_intersections(element_1, element_2);
    }
    
    throw std::invalid_argument("irregular::compute_intersections: Invalid element types");
}

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
