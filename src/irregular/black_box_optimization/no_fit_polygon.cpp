#include "irregular/black_box_optimization/no_fit_polygon.hpp"

#include "shape/approximation.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/no_fit_polygon.hpp"

#include <cmath>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/** Segment length of the approximation of the circular arcs, if any. */
const LengthDbl arc_segment_length = 1.0;

/** Below this distance, a point is considered on the boundary. */
const LengthDbl boundary_tolerance = 1e-12;

NoFitPolygon::Ring build_ring(
        const Shape& shape,
        bool is_hole)
{
    NoFitPolygon::Ring ring;
    for (const ShapeElement& element: shape.elements) {
        ring.vertices.push_back(element.start);
        ring.aabb.x_min = (std::min)(ring.aabb.x_min, element.start.x);
        ring.aabb.x_max = (std::max)(ring.aabb.x_max, element.start.x);
        ring.aabb.y_min = (std::min)(ring.aabb.y_min, element.start.y);
        ring.aabb.y_max = (std::max)(ring.aabb.y_max, element.start.y);
    }
    AreaDbl area = 0.0;
    for (ElementPos vertex_pos = 0;
            vertex_pos < (ElementPos)ring.vertices.size();
            ++vertex_pos) {
        const Point& vertex_1 = ring.vertices[vertex_pos];
        const Point& vertex_2 = ring.vertices[(vertex_pos + 1) % ring.vertices.size()];
        area += vertex_1.x * vertex_2.y - vertex_2.x * vertex_1.y;
    }
    // The interior of the no-fit polygon is inside its outlines and outside
    // its holes.
    ring.interior_on_left = (area > 0) != is_hole;
    return ring;
}

/** Return true iff a point is strictly inside a ring (crossing number). */
bool inside_ring(
        const NoFitPolygon::Ring& ring,
        const Point& point)
{
    if (point.x < ring.aabb.x_min
            || point.x > ring.aabb.x_max
            || point.y < ring.aabb.y_min
            || point.y > ring.aabb.y_max) {
        return false;
    }
    bool inside = false;
    ElementPos number_of_vertices = ring.vertices.size();
    for (ElementPos vertex_pos = 0, previous_vertex_pos = number_of_vertices - 1;
            vertex_pos < number_of_vertices;
            previous_vertex_pos = vertex_pos++) {
        const Point& vertex_1 = ring.vertices[previous_vertex_pos];
        const Point& vertex_2 = ring.vertices[vertex_pos];
        if ((vertex_1.y > point.y) != (vertex_2.y > point.y)) {
            LengthDbl x = vertex_1.x
                + (point.y - vertex_1.y) * (vertex_2.x - vertex_1.x) / (vertex_2.y - vertex_1.y);
            if (point.x < x)
                inside = !inside;
        }
    }
    return inside;
}

/** Distance from a point to a bounding box (0 inside). */
LengthDbl aabb_distance(
        const AxisAlignedBoundingBox& aabb,
        const Point& point)
{
    LengthDbl distance_x = (std::max)({aabb.x_min - point.x, 0.0, point.x - aabb.x_max});
    LengthDbl distance_y = (std::max)({aabb.y_min - point.y, 0.0, point.y - aabb.y_max});
    return std::sqrt(distance_x * distance_x + distance_y * distance_y);
}

}

NoFitPolygon packingsolver::irregular::build_no_fit_polygon(
        const MultiShapeWithHoles& shapes)
{
    NoFitPolygon no_fit_polygon;
    for (const ShapeWithHoles& shape_with_holes: shapes.shapes_with_holes) {
        bool has_arcs = false;
        for (const ShapeElement& element: shape_with_holes.shape.elements)
            if (element.type != ShapeElementType::LineSegment)
                has_arcs = true;
        for (const Shape& hole: shape_with_holes.holes)
            for (const ShapeElement& element: hole.elements)
                if (element.type != ShapeElementType::LineSegment)
                    has_arcs = true;
        const ShapeWithHoles polygon = (has_arcs)?
            approximate_by_line_segments(shape_with_holes, arc_segment_length):
            shape_with_holes;

        NoFitPolygon::Component component;
        component.rings.push_back(build_ring(polygon.shape, false));
        for (const Shape& hole: polygon.holes)
            component.rings.push_back(build_ring(hole, true));
        no_fit_polygon.aabb = merge(no_fit_polygon.aabb, component.rings.front().aabb);
        no_fit_polygon.components.push_back(component);
    }
    return no_fit_polygon;
}

NoFitPolygon packingsolver::irregular::build_no_fit_polygon(
        const std::vector<ShapeWithHoles>& fixed_shapes,
        const std::vector<ShapeWithHoles>& orbiting_shapes)
{
    std::vector<ShapeWithHoles> parts;
    for (const ShapeWithHoles& fixed_shape: fixed_shapes) {
        for (const ShapeWithHoles& orbiting_shape: orbiting_shapes) {
            MultiShapeWithHoles part = shape::no_fit_polygon(fixed_shape, orbiting_shape);
            parts.insert(
                    parts.end(),
                    part.shapes_with_holes.begin(),
                    part.shapes_with_holes.end());
        }
    }
    if (parts.size() == 1)
        return build_no_fit_polygon(MultiShapeWithHoles{parts});
    return build_no_fit_polygon(compute_union(parts));
}

bool packingsolver::irregular::signed_distance(
        const NoFitPolygon& no_fit_polygon,
        const Point& point,
        LengthDbl maximum_outside_distance,
        SignedDistance& output)
{
    if (aabb_distance(no_fit_polygon.aabb, point) >= maximum_outside_distance)
        return false;

    // Location of the point: inside the outline of a component, and outside
    // its holes. The components are disjoint.
    bool inside = false;
    for (const NoFitPolygon::Component& component: no_fit_polygon.components) {
        if (!inside_ring(component.rings.front(), point))
            continue;
        bool in_hole = false;
        for (ElementPos ring_pos = 1;
                ring_pos < (ElementPos)component.rings.size();
                ++ring_pos) {
            if (inside_ring(component.rings[ring_pos], point)) {
                in_hole = true;
                break;
            }
        }
        if (!in_hole) {
            inside = true;
            break;
        }
    }

    // Closest point of the boundary.
    LengthDbl distance_best = (inside)?
        std::numeric_limits<LengthDbl>::infinity():
        maximum_outside_distance;
    const NoFitPolygon::Ring* ring_best = nullptr;
    Point closest_point_best = {0.0, 0.0};
    Point edge_best = {0.0, 0.0};
    for (const NoFitPolygon::Component& component: no_fit_polygon.components) {
        for (const NoFitPolygon::Ring& ring: component.rings) {
            if (aabb_distance(ring.aabb, point) >= distance_best)
                continue;
            ElementPos number_of_vertices = ring.vertices.size();
            for (ElementPos vertex_pos = 0;
                    vertex_pos < number_of_vertices;
                    ++vertex_pos) {
                const Point& vertex_1 = ring.vertices[vertex_pos];
                const Point& vertex_2 = ring.vertices[(vertex_pos + 1) % number_of_vertices];
                Point edge = {vertex_2.x - vertex_1.x, vertex_2.y - vertex_1.y};
                LengthDbl edge_squared_length = edge.x * edge.x + edge.y * edge.y;
                double ratio = 0.0;
                if (edge_squared_length > 0.0) {
                    ratio = ((point.x - vertex_1.x) * edge.x
                            + (point.y - vertex_1.y) * edge.y) / edge_squared_length;
                    ratio = (std::max)(0.0, (std::min)(1.0, ratio));
                }
                Point closest_point = {
                    vertex_1.x + ratio * edge.x,
                    vertex_1.y + ratio * edge.y};
                LengthDbl distance = std::sqrt(
                        (point.x - closest_point.x) * (point.x - closest_point.x)
                        + (point.y - closest_point.y) * (point.y - closest_point.y));
                if (distance < distance_best) {
                    distance_best = distance;
                    ring_best = &ring;
                    closest_point_best = closest_point;
                    edge_best = edge;
                }
            }
        }
    }
    if (ring_best == nullptr)
        return false;

    output.distance = (inside)? distance_best: -distance_best;
    if (distance_best > boundary_tolerance) {
        // The signed distance increases away from the closest point inside,
        // towards it outside.
        double sign = (inside)? 1.0: -1.0;
        output.gradient = {
            sign * (point.x - closest_point_best.x) / distance_best,
            sign * (point.y - closest_point_best.y) / distance_best};
    } else {
        // On the boundary: the normal of the edge, towards the interior.
        LengthDbl edge_length = std::sqrt(edge_best.x * edge_best.x + edge_best.y * edge_best.y);
        if (edge_length == 0.0) {
            output.gradient = {0.0, 0.0};
            return true;
        }
        double sign = (ring_best->interior_on_left)? 1.0: -1.0;
        output.gradient = {
            -sign * edge_best.y / edge_length,
            sign * edge_best.x / edge_length};
    }
    return true;
}
