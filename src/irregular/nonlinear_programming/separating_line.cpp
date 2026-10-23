#include "irregular/nonlinear_programming/separating_line.hpp"

#include <cmath>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Rotate the vertices of a convex polygon so that it starts with its lowest
 * vertex (the leftmost one in case of tie).
 */
std::vector<Point> start_at_lowest_vertex(const std::vector<Point>& polygon)
{
    ElementPos lowest_vertex_pos = 0;
    for (ElementPos vertex_pos = 1; vertex_pos < (ElementPos)polygon.size(); ++vertex_pos) {
        const Point& vertex = polygon[vertex_pos];
        const Point& lowest_vertex = polygon[lowest_vertex_pos];
        if (vertex.y < lowest_vertex.y
                || (vertex.y == lowest_vertex.y && vertex.x < lowest_vertex.x)) {
            lowest_vertex_pos = vertex_pos;
        }
    }
    std::vector<Point> rotated_polygon;
    for (ElementPos offset = 0; offset < (ElementPos)polygon.size(); ++offset)
        rotated_polygon.push_back(polygon[(lowest_vertex_pos + offset) % polygon.size()]);
    return rotated_polygon;
}

/** Minkowski sum of two convex polygons given anticlockwise. */
std::vector<Point> minkowski_sum(
        const std::vector<Point>& polygon_1,
        const std::vector<Point>& polygon_2)
{
    std::vector<Point> vertices_1 = start_at_lowest_vertex(polygon_1);
    std::vector<Point> vertices_2 = start_at_lowest_vertex(polygon_2);
    ElementPos number_of_vertices_1 = vertices_1.size();
    ElementPos number_of_vertices_2 = vertices_2.size();
    vertices_1.push_back(vertices_1[0]);
    vertices_1.push_back(vertices_1[1 % number_of_vertices_1]);
    vertices_2.push_back(vertices_2[0]);
    vertices_2.push_back(vertices_2[1 % number_of_vertices_2]);

    // Merge the edges sorted by angle.
    std::vector<Point> sum;
    ElementPos vertex_pos_1 = 0;
    ElementPos vertex_pos_2 = 0;
    while (vertex_pos_1 < number_of_vertices_1 || vertex_pos_2 < number_of_vertices_2) {
        sum.push_back(vertices_1[vertex_pos_1] + vertices_2[vertex_pos_2]);
        LengthDbl cross = cross_product(
                vertices_1[vertex_pos_1 + 1] - vertices_1[vertex_pos_1],
                vertices_2[vertex_pos_2 + 1] - vertices_2[vertex_pos_2]);
        bool advance_1 = (cross >= 0 && vertex_pos_1 < number_of_vertices_1);
        bool advance_2 = (cross <= 0 && vertex_pos_2 < number_of_vertices_2);
        // With (almost) degenerate polygons, the cross product may not allow
        // advancing: advance anyway.
        if (!advance_1 && !advance_2) {
            if (vertex_pos_1 < number_of_vertices_1) {
                advance_1 = true;
            } else {
                advance_2 = true;
            }
        }
        if (advance_1)
            ++vertex_pos_1;
        if (advance_2)
            ++vertex_pos_2;
    }
    return sum;
}

}

SeparatingLine packingsolver::irregular::compute_separating_line(
        const std::vector<Point>& polygon_1,
        const std::vector<Point>& polygon_2)
{
    const LengthDbl infinity = std::numeric_limits<LengthDbl>::infinity();

    // Minkowski difference polygon_1 - polygon_2.
    std::vector<Point> opposite_polygon_2;
    for (const Point& vertex: polygon_2)
        opposite_polygon_2.push_back({-vertex.x, -vertex.y});
    std::vector<Point> difference = minkowski_sum(polygon_1, opposite_polygon_2);
    ElementPos number_of_vertices = difference.size();

    // Edge of the difference whose line is the closest to the origin (on its
    // inner side), and point of the difference closest to the origin.
    bool origin_inside = true;
    LengthDbl smallest_edge_distance = infinity;
    Point smallest_edge_normal = {1.0, 0.0};
    LengthDbl smallest_distance = infinity;
    Point closest_point = {0.0, 0.0};
    Point closest_edge_normal = {1.0, 0.0};
    LengthDbl largest_length = 0.0;
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        Point edge = difference[(vertex_pos + 1) % number_of_vertices] - difference[vertex_pos];
        largest_length = (std::max)(largest_length, std::sqrt(edge.x * edge.x + edge.y * edge.y));
    }
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        const Point& edge_start = difference[vertex_pos];
        const Point& edge_end = difference[(vertex_pos + 1) % number_of_vertices];
        Point edge = edge_end - edge_start;
        LengthDbl length = std::sqrt(edge.x * edge.x + edge.y * edge.y);
        // The normals of the negligible edges (of polygons reduced to
        // almost a point) are inaccurate.
        if (length == 0.0 || length <= 1e-9 * largest_length)
            continue;

        // The difference is anticlockwise: its outward normal is on the right
        // of the edge.
        Point normal = {edge.y / length, -edge.x / length};
        LengthDbl edge_distance = normal.x * edge_start.x + normal.y * edge_start.y;
        if (edge_distance < 0)
            origin_inside = false;
        if (edge_distance < smallest_edge_distance) {
            smallest_edge_distance = edge_distance;
            smallest_edge_normal = normal;
        }

        // Closest point of the edge to the origin.
        LengthDbl ratio = -(edge_start.x * edge.x + edge_start.y * edge.y) / (length * length);
        ratio = (std::max)(0.0, (std::min)(1.0, ratio));
        Point point = {edge_start.x + ratio * edge.x, edge_start.y + ratio * edge.y};
        LengthDbl distance = std::sqrt(point.x * point.x + point.y * point.y);
        if (distance < smallest_distance) {
            smallest_distance = distance;
            closest_point = point;
            closest_edge_normal = normal;
        }
    }

    // A difference without area (when a polygon is reduced to a point or a
    // segment) can't contain the origin.
    AreaDbl doubled_area = 0.0;
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        doubled_area += cross_product(
                difference[vertex_pos],
                difference[(vertex_pos + 1) % number_of_vertices]);
    }
    if (!(doubled_area > 1e-12 * largest_length * largest_length))
        origin_inside = false;

    // Both polygons reduced to points.
    if (largest_length == 0.0) {
        closest_point = difference[0];
        smallest_distance = std::sqrt(closest_point.x * closest_point.x + closest_point.y * closest_point.y);
    }

    // If the origin is on the boundary of the difference (the polygons
    // touch), the normal is the normal of the edge containing it.
    SeparatingLine separating_line;
    if (origin_inside) {
        separating_line.normal = smallest_edge_normal;
    } else if (smallest_distance <= 1e-12 * largest_length) {
        separating_line.normal = closest_edge_normal;
    } else {
        separating_line.normal = {
            -closest_point.x / smallest_distance,
            -closest_point.y / smallest_distance};
    }

    // Offset halfway between the polygons.
    LengthDbl maximum_1 = -infinity;
    for (const Point& vertex: polygon_1) {
        maximum_1 = (std::max)(
                maximum_1,
                separating_line.normal.x * vertex.x + separating_line.normal.y * vertex.y);
    }
    LengthDbl minimum_2 = infinity;
    for (const Point& vertex: polygon_2) {
        minimum_2 = (std::min)(
                minimum_2,
                separating_line.normal.x * vertex.x + separating_line.normal.y * vertex.y);
    }
    separating_line.offset = (maximum_1 + minimum_2) / 2;
    separating_line.gap = minimum_2 - maximum_1;
    return separating_line;
}
