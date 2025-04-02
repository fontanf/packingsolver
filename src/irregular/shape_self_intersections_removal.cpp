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
