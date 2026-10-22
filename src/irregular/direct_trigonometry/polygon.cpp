#include "irregular/direct_trigonometry/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <deque>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Tolerance on distances, in scaled coordinates (the scaled bins fit in a
 * 1024 x 1024 square): penetrations smaller than it are not overlaps.
 */
const LengthDbl tolerance = 1e-7;

/** Distance from 'point' to the segment from 'segment_start' to 'segment_end'. */
inline LengthDbl distance_to_segment(
        const Point& segment_start,
        const Point& segment_end,
        const Point& point)
{
    LengthDbl delta_x = segment_end.x - segment_start.x;
    LengthDbl delta_y = segment_end.y - segment_start.y;
    LengthDbl squared_length = delta_x * delta_x + delta_y * delta_y;
    LengthDbl ratio = 0.0;
    if (squared_length > 0.0) {
        ratio = ((point.x - segment_start.x) * delta_x + (point.y - segment_start.y) * delta_y) / squared_length;
        ratio = (std::max)(0.0, (std::min)(1.0, ratio));
    }
    LengthDbl projection_x = segment_start.x + ratio * delta_x;
    LengthDbl projection_y = segment_start.y + ratio * delta_y;
    return std::sqrt(
            (point.x - projection_x) * (point.x - projection_x)
            + (point.y - projection_y) * (point.y - projection_y));
}

/** Parameter of the projection of 'point' on the segment, 0 at its start, 1 at its end. */
inline double projection_parameter(
        const Point& segment_start,
        const Point& segment_end,
        const Point& point)
{
    LengthDbl delta_x = segment_end.x - segment_start.x;
    LengthDbl delta_y = segment_end.y - segment_start.y;
    LengthDbl squared_length = delta_x * delta_x + delta_y * delta_y;
    if (squared_length == 0.0)
        return 0.0;
    return ((point.x - segment_start.x) * delta_x + (point.y - segment_start.y) * delta_y) / squared_length;
}

/**
 * Cross product of ('segment_end' - 'segment_start') and ('point' -
 * 'segment_start'): the signed distance from 'point' to the line through the
 * segment, times the length of the segment.
 */
inline LengthDbl cross_product(
        const Point& segment_start,
        const Point& segment_end,
        const Point& point)
{
    return (segment_end.x - segment_start.x) * (point.y - segment_start.y)
        - (segment_end.y - segment_start.y) * (point.x - segment_start.x);
}

/** Return true iff 'point' is on the segment, up to the tolerance. */
inline bool point_on_segment(
        const Point& segment_start,
        const Point& segment_end,
        const Point& point)
{
    if (point.x < (std::min)(segment_start.x, segment_end.x) - tolerance
            || point.x > (std::max)(segment_start.x, segment_end.x) + tolerance
            || point.y < (std::min)(segment_start.y, segment_end.y) - tolerance
            || point.y > (std::max)(segment_start.y, segment_end.y) + tolerance) {
        return false;
    }
    LengthDbl delta_x = segment_end.x - segment_start.x;
    LengthDbl delta_y = segment_end.y - segment_start.y;
    LengthDbl squared_length = delta_x * delta_x + delta_y * delta_y;
    LengthDbl dot = (point.x - segment_start.x) * delta_x + (point.y - segment_start.y) * delta_y;
    if (dot >= 0 && dot <= squared_length) {
        LengthDbl cross = cross_product(segment_start, segment_end, point);
        return cross * cross <= tolerance * tolerance * squared_length;
    }
    // Closest point: one of the ends.
    const Point& end = (dot < 0)? segment_start: segment_end;
    return (point.x - end.x) * (point.x - end.x) + (point.y - end.y) * (point.y - end.y)
        <= tolerance * tolerance;
}

/** Bounding box of a segment. */
inline AxisAlignedBoundingBox segment_aabb(
        const Point& segment_start,
        const Point& segment_end)
{
    AxisAlignedBoundingBox aabb;
    aabb.x_min = (std::min)(segment_start.x, segment_end.x);
    aabb.x_max = (std::max)(segment_start.x, segment_end.x);
    aabb.y_min = (std::min)(segment_start.y, segment_end.y);
    aabb.y_max = (std::max)(segment_start.y, segment_end.y);
    return aabb;
}

/** Return true iff the two boxes intersect, up to the tolerance. */
inline bool aabbs_intersect(
        const AxisAlignedBoundingBox& aabb_1,
        const AxisAlignedBoundingBox& aabb_2)
{
    return aabb_1.x_min <= aabb_2.x_max + tolerance
        && aabb_2.x_min <= aabb_1.x_max + tolerance
        && aabb_1.y_min <= aabb_2.y_max + tolerance
        && aabb_2.y_min <= aabb_1.y_max + tolerance;
}

inline Point translate(
        const Point& point,
        const Point& translation)
{
    return {point.x + translation.x, point.y + translation.y};
}

inline AxisAlignedBoundingBox translate(
        const AxisAlignedBoundingBox& aabb,
        const Point& translation)
{
    AxisAlignedBoundingBox aabb_new;
    aabb_new.x_min = aabb.x_min + translation.x;
    aabb_new.x_max = aabb.x_max + translation.x;
    aabb_new.y_min = aabb.y_min + translation.y;
    aabb_new.y_max = aabb.y_max + translation.y;
    return aabb_new;
}

/** Locate 'point' - 'translation' relatively to 'polygon'. */
PointLocation locate_translated_point(
        const Point& point,
        const Polygon& polygon,
        const Point& translation)
{
    Point point_local = {point.x - translation.x, point.y - translation.y};
    return locate_point(point_local, polygon);
}

/**
 * Return true iff the boundary of 'polygon_1' translated by 'translation_1'
 * enters the interior of 'polygon_2' (not translated), considering only the
 * parts of the boundary inside 'window'.
 *
 * Each edge of 'polygon_1' is split at its contacts with the boundary of
 * 'polygon_2'; between two consecutive contacts, the edge is either entirely
 * inside 'polygon_2', or entirely outside, or on its boundary; therefore,
 * testing the middle of each part is enough.
 *
 * Two consecutive edges without contacts, whose common vertex isn't on the
 * boundary of 'polygon_2', are on the same side of it: the middle of only one
 * of them is tested.
 */
bool boundary_enters(
        const Polygon& polygon_1,
        const Point& translation_1,
        const Polygon& polygon_2,
        const AxisAlignedBoundingBox& window)
{
    struct Edge
    {
        Point start;
        Point end;
        LengthDbl length;
        AxisAlignedBoundingBox aabb;
    };

    // Buffers, reused between calls to avoid allocations.
    static thread_local std::vector<Edge> edges_2;
    static thread_local std::vector<double> contacts;

    // Edges of 'polygon_2' in the window.
    edges_2.clear();
    ElementPos number_of_vertices_2 = polygon_2.vertices.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices_2; ++vertex_pos) {
        Edge edge;
        edge.start = polygon_2.vertices[vertex_pos];
        edge.end = polygon_2.vertices[(vertex_pos + 1) % number_of_vertices_2];
        edge.aabb = segment_aabb(edge.start, edge.end);
        if (!aabbs_intersect(edge.aabb, window))
            continue;
        edge.length = polygon_2.edge_lengths[vertex_pos];
        edges_2.push_back(edge);
    }

    // Location of the previous edge, if it has no contact and its end isn't
    // on the boundary of 'polygon_2'.
    PointLocation chain_location = PointLocation::Outside;
    bool chain_valid = false;
    ElementPos number_of_vertices_1 = polygon_1.vertices.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices_1; ++vertex_pos) {
        Point edge_1_start = translate(polygon_1.vertices[vertex_pos], translation_1);
        Point edge_1_end = translate(polygon_1.vertices[(vertex_pos + 1) % number_of_vertices_1], translation_1);
        AxisAlignedBoundingBox edge_1_aabb = segment_aabb(edge_1_start, edge_1_end);
        if (!aabbs_intersect(edge_1_aabb, window)) {
            chain_valid = false;
            continue;
        }
        LengthDbl edge_1_length = polygon_1.edge_lengths[vertex_pos];
        LengthDbl tolerance_1 = tolerance * edge_1_length;

        contacts.clear();
        contacts.push_back(0.0);
        contacts.push_back(1.0);
        bool start_on_boundary = false;
        bool end_on_boundary = false;
        for (const Edge& edge_2: edges_2) {
            if (!aabbs_intersect(edge_1_aabb, edge_2.aabb))
                continue;
            LengthDbl tolerance_2 = tolerance * edge_2.length;
            LengthDbl cross_1_start = cross_product(edge_2.start, edge_2.end, edge_1_start);
            LengthDbl cross_1_end = cross_product(edge_2.start, edge_2.end, edge_1_end);
            LengthDbl cross_2_start = cross_product(edge_1_start, edge_1_end, edge_2.start);
            LengthDbl cross_2_end = cross_product(edge_1_start, edge_1_end, edge_2.end);

            // Proper crossing.
            if (((cross_1_start > tolerance_2 && cross_1_end < -tolerance_2)
                        || (cross_1_start < -tolerance_2 && cross_1_end > tolerance_2))
                    && ((cross_2_start > tolerance_1 && cross_2_end < -tolerance_1)
                        || (cross_2_start < -tolerance_1 && cross_2_end > tolerance_1))) {
                return true;
            }

            // Contacts: endpoints of the edge of 'polygon_2' on the edge of
            // 'polygon_1'.
            if (std::abs(cross_2_start) <= tolerance_1
                    && point_on_segment(edge_1_start, edge_1_end, edge_2.start)) {
                contacts.push_back(projection_parameter(edge_1_start, edge_1_end, edge_2.start));
            }
            if (std::abs(cross_2_end) <= tolerance_1
                    && point_on_segment(edge_1_start, edge_1_end, edge_2.end)) {
                contacts.push_back(projection_parameter(edge_1_start, edge_1_end, edge_2.end));
            }

            // Endpoints of the edge of 'polygon_1' on the boundary of
            // 'polygon_2'.
            if (std::abs(cross_1_start) <= tolerance_2
                    && point_on_segment(edge_2.start, edge_2.end, edge_1_start)) {
                start_on_boundary = true;
            }
            if (std::abs(cross_1_end) <= tolerance_2
                    && point_on_segment(edge_2.start, edge_2.end, edge_1_end)) {
                end_on_boundary = true;
            }
        }

        // Edge without contact: entirely on one side.
        if (contacts.size() == 2 && !start_on_boundary && !end_on_boundary) {
            PointLocation location = chain_location;
            if (!chain_valid) {
                Point middle = {
                    (edge_1_start.x + edge_1_end.x) / 2,
                    (edge_1_start.y + edge_1_end.y) / 2};
                location = locate_point(middle, polygon_2);
            }
            if (location == PointLocation::Inside)
                return true;
            chain_location = location;
            chain_valid = (location != PointLocation::Boundary);
            continue;
        }
        chain_valid = false;

        // Test the middle of each part of the edge.
        std::sort(contacts.begin(), contacts.end());
        for (Counter contact_pos = 0;
                contact_pos + 1 < (Counter)contacts.size();
                ++contact_pos) {
            double ratio_start = (std::max)(0.0, contacts[contact_pos]);
            double ratio_end = (std::min)(1.0, contacts[contact_pos + 1]);
            if ((ratio_end - ratio_start) * edge_1_length <= 2 * tolerance)
                continue;
            double ratio_middle = (ratio_start + ratio_end) / 2;
            Point middle = {
                edge_1_start.x + ratio_middle * (edge_1_end.x - edge_1_start.x),
                edge_1_start.y + ratio_middle * (edge_1_end.y - edge_1_start.y)};
            if (locate_point(middle, polygon_2) == PointLocation::Inside)
                return true;
        }
    }
    return false;
}

/**
 * Signed distance from 'point' to the boundary of the region inside
 * 'polygon' and outside 'poles': positive inside the region, negative
 * outside.
 */
LengthDbl signed_distance_to_region(
        const Polygon& polygon,
        const std::vector<Pole>& poles,
        const Point& point)
{
    LengthDbl distance = std::numeric_limits<LengthDbl>::infinity();
    ElementPos number_of_vertices = polygon.vertices.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        distance = (std::min)(distance, distance_to_segment(
                    polygon.vertices[vertex_pos],
                    polygon.vertices[(vertex_pos + 1) % number_of_vertices],
                    point));
    }
    bool inside = (locate_point(point, polygon) == PointLocation::Inside);
    for (const Pole& pole: poles) {
        LengthDbl distance_to_center = std::sqrt(
                (point.x - pole.center.x) * (point.x - pole.center.x)
                + (point.y - pole.center.y) * (point.y - pole.center.y));
        LengthDbl distance_to_pole = distance_to_center - pole.radius;
        if (distance_to_pole < 0)
            inside = false;
        distance = (std::min)(distance, std::abs(distance_to_pole));
    }
    return (inside)? distance: -distance;
}

/**
 * Compute the largest circle inscribed in 'polygon' and disjoint from
 * 'poles'.
 *
 * Based on Mapbox's "polylabel" algorithm: a quadtree search in which a
 * square cell is only split if its center's distance plus its half-diagonal
 * (an upper bound of the distance of any point of the cell) can improve the
 * best distance found.
 */
Pole compute_next_pole(
        const Polygon& polygon,
        const std::vector<Pole>& poles)
{
    struct Cell
    {
        Point center;
        LengthDbl half_size;
        LengthDbl distance;
        Counter depth;
    };
    const Counter maximum_depth = 10;
    const LengthDbl sqrt_2 = std::sqrt(2.0);

    LengthDbl size = (std::max)(
            polygon.aabb.x_max - polygon.aabb.x_min,
            polygon.aabb.y_max - polygon.aabb.y_min);
    Point center = {
        (polygon.aabb.x_min + polygon.aabb.x_max) / 2,
        (polygon.aabb.y_min + polygon.aabb.y_max) / 2};

    Pole best;
    std::deque<Cell> queue;
    queue.push_back({center, size / 2, signed_distance_to_region(polygon, poles, center), 0});
    while (!queue.empty()) {
        Cell cell = queue.front();
        queue.pop_front();
        if (cell.distance > best.radius) {
            best.center = cell.center;
            best.radius = cell.distance;
        }
        if (cell.depth == maximum_depth)
            continue;
        if (cell.distance + cell.half_size * sqrt_2 <= best.radius)
            continue;
        LengthDbl half_size_child = cell.half_size / 2;
        for (int child_pos = 0; child_pos < 4; ++child_pos) {
            Point center_child = {
                cell.center.x + ((child_pos % 2 == 0)? -half_size_child: half_size_child),
                cell.center.y + ((child_pos / 2 == 0)? -half_size_child: half_size_child)};
            queue.push_back({
                    center_child,
                    half_size_child,
                    signed_distance_to_region(polygon, poles, center_child),
                    cell.depth + 1});
        }
    }
    return best;
}

/**
 * Compute a point strictly inside a simple polygon, in linear time.
 *
 * The lowest-leftmost vertex is convex. If no other vertex is inside the
 * triangle formed by it and its neighbors, the centroid of the triangle is
 * inside the polygon. Otherwise, the segment from it to the vertex inside the
 * triangle the farthest from the line through its neighbors is a diagonal of
 * the polygon, and its middle is inside the polygon.
 */
Point compute_interior_point(const std::vector<Point>& vertices)
{
    ElementPos number_of_vertices = vertices.size();
    ElementPos vertex_pos = 0;
    for (ElementPos vertex_pos_cur = 1; vertex_pos_cur < number_of_vertices; ++vertex_pos_cur) {
        const Point& vertex = vertices[vertex_pos_cur];
        const Point& vertex_best = vertices[vertex_pos];
        if (vertex.y < vertex_best.y
                || (vertex.y == vertex_best.y && vertex.x < vertex_best.x)) {
            vertex_pos = vertex_pos_cur;
        }
    }
    const Point& vertex = vertices[vertex_pos];
    const Point& vertex_previous = vertices[(vertex_pos + number_of_vertices - 1) % number_of_vertices];
    const Point& vertex_next = vertices[(vertex_pos + 1) % number_of_vertices];

    // Vertices inside the triangle.
    LengthDbl orientation = cross_product(vertex_previous, vertex, vertex_next);
    ElementPos farthest_vertex_pos = -1;
    LengthDbl farthest_distance = -1;
    for (ElementPos vertex_pos_cur = 0; vertex_pos_cur < number_of_vertices; ++vertex_pos_cur) {
        const Point& vertex_cur = vertices[vertex_pos_cur];
        if (vertex_pos_cur == vertex_pos
                || (vertex_cur.x == vertex_previous.x && vertex_cur.y == vertex_previous.y)
                || (vertex_cur.x == vertex_next.x && vertex_cur.y == vertex_next.y)
                || (vertex_cur.x == vertex.x && vertex_cur.y == vertex.y)) {
            continue;
        }
        LengthDbl cross_1 = cross_product(vertex_previous, vertex, vertex_cur);
        LengthDbl cross_2 = cross_product(vertex, vertex_next, vertex_cur);
        LengthDbl cross_3 = cross_product(vertex_next, vertex_previous, vertex_cur);
        bool inside = (orientation > 0)?
            (cross_1 >= 0 && cross_2 >= 0 && cross_3 >= 0):
            (cross_1 <= 0 && cross_2 <= 0 && cross_3 <= 0);
        if (!inside)
            continue;
        // Distance to the line through the neighbors.
        LengthDbl distance = std::abs(cross_product(vertex_previous, vertex_next, vertex_cur));
        if (distance > farthest_distance) {
            farthest_distance = distance;
            farthest_vertex_pos = vertex_pos_cur;
        }
    }
    if (farthest_vertex_pos == -1) {
        return {
            (vertex_previous.x + vertex.x + vertex_next.x) / 3,
            (vertex_previous.y + vertex.y + vertex_next.y) / 3};
    }
    const Point& farthest_vertex = vertices[farthest_vertex_pos];
    return {(vertex.x + farthest_vertex.x) / 2, (vertex.y + farthest_vertex.y) / 2};
}

/** Convex hull of a set of points, anticlockwise (Andrew's monotone chain). */
std::vector<Point> compute_convex_hull(std::vector<Point> points)
{
    std::sort(
            points.begin(),
            points.end(),
            [](const Point& point_1, const Point& point_2)
            {
                return (point_1.x != point_2.x)?
                    point_1.x < point_2.x:
                    point_1.y < point_2.y;
            });
    if (points.size() < 3)
        return points;
    std::vector<Point> hull(2 * points.size());
    ElementPos hull_size = 0;
    for (ElementPos point_pos = 0; point_pos < (ElementPos)points.size(); ++point_pos) {
        while (hull_size >= 2
                && cross_product(hull[hull_size - 2], hull[hull_size - 1], points[point_pos]) <= 0) {
            hull_size--;
        }
        hull[hull_size++] = points[point_pos];
    }
    ElementPos lower_size = hull_size + 1;
    for (ElementPos point_pos = (ElementPos)points.size() - 2; point_pos >= 0; --point_pos) {
        while (hull_size >= lower_size
                && cross_product(hull[hull_size - 2], hull[hull_size - 1], points[point_pos]) <= 0) {
            hull_size--;
        }
        hull[hull_size++] = points[point_pos];
    }
    hull.resize(hull_size - 1);
    return hull;
}

/**
 * Return true iff an edge of the convex hull 'hull_1' separates it from the
 * convex hull 'hull_2' translated by 'translation' (by more than the
 * tolerance).
 */
bool separated_by_an_edge(
        const std::vector<Point>& hull_1,
        const std::vector<Point>& hull_2,
        const Point& translation)
{
    ElementPos number_of_vertices_1 = hull_1.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices_1; ++vertex_pos) {
        const Point& edge_start = hull_1[vertex_pos];
        const Point& edge_end = hull_1[(vertex_pos + 1) % number_of_vertices_1];
        LengthDbl delta_x = edge_end.x - edge_start.x;
        LengthDbl delta_y = edge_end.y - edge_start.y;
        LengthDbl tolerance_edge = -tolerance * std::sqrt(delta_x * delta_x + delta_y * delta_y);
        // The hull is anticlockwise: its interior is on the left of each
        // edge; 'hull_2' is separated if it is entirely on the right.
        bool separated = true;
        for (const Point& vertex: hull_2) {
            Point vertex_translated = {vertex.x + translation.x, vertex.y + translation.y};
            if (cross_product(edge_start, edge_end, vertex_translated) > tolerance_edge) {
                separated = false;
                break;
            }
        }
        if (separated)
            return true;
    }
    return false;
}

}

Polygon packingsolver::irregular::build_polygon(const Shape& shape)
{
    Polygon polygon;
    for (const ShapeElement& element: shape.elements) {
        if (element.type != ShapeElementType::LineSegment) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "the elements of the shape must be line segments.");
        }
        polygon.vertices.push_back(element.start);
        polygon.aabb.x_min = (std::min)(polygon.aabb.x_min, element.start.x);
        polygon.aabb.x_max = (std::max)(polygon.aabb.x_max, element.start.x);
        polygon.aabb.y_min = (std::min)(polygon.aabb.y_min, element.start.y);
        polygon.aabb.y_max = (std::max)(polygon.aabb.y_max, element.start.y);
    }
    ElementPos number_of_vertices = polygon.vertices.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        const Point& edge_start = polygon.vertices[vertex_pos];
        const Point& edge_end = polygon.vertices[(vertex_pos + 1) % number_of_vertices];
        polygon.edge_lengths.push_back(std::sqrt(
                    (edge_end.x - edge_start.x) * (edge_end.x - edge_start.x)
                    + (edge_end.y - edge_start.y) * (edge_end.y - edge_start.y)));
    }
    polygon.interior_point = compute_interior_point(polygon.vertices);
    polygon.convex_hull = compute_convex_hull(polygon.vertices);
    return polygon;
}

PointLocation packingsolver::irregular::locate_point(
        const Point& point,
        const Polygon& polygon)
{
    if (point.x < polygon.aabb.x_min - tolerance
            || point.x > polygon.aabb.x_max + tolerance
            || point.y < polygon.aabb.y_min - tolerance
            || point.y > polygon.aabb.y_max + tolerance) {
        return PointLocation::Outside;
    }
    bool inside = false;
    ElementPos number_of_vertices = polygon.vertices.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        const Point& edge_start = polygon.vertices[vertex_pos];
        const Point& edge_end = polygon.vertices[(vertex_pos + 1) % number_of_vertices];
        if (point_on_segment(edge_start, edge_end, point))
            return PointLocation::Boundary;
        if ((edge_start.y > point.y) != (edge_end.y > point.y)) {
            LengthDbl x_intersection = edge_start.x
                + (point.y - edge_start.y) * (edge_end.x - edge_start.x) / (edge_end.y - edge_start.y);
            if (point.x < x_intersection)
                inside = !inside;
        }
    }
    return (inside)? PointLocation::Inside: PointLocation::Outside;
}

bool packingsolver::irregular::intersect(
        const Polygon& polygon_1,
        const Point& translation_1,
        const Polygon& polygon_2,
        const Point& translation_2)
{
    // Work in the frame of 'polygon_2'.
    Point translation = {
        translation_1.x - translation_2.x,
        translation_1.y - translation_2.y};
    AxisAlignedBoundingBox aabb_1 = translate(polygon_1.aabb, translation);
    const AxisAlignedBoundingBox& aabb_2 = polygon_2.aabb;
    AxisAlignedBoundingBox window;
    window.x_min = (std::max)(aabb_1.x_min, aabb_2.x_min);
    window.x_max = (std::min)(aabb_1.x_max, aabb_2.x_max);
    window.y_min = (std::max)(aabb_1.y_min, aabb_2.y_min);
    window.y_max = (std::min)(aabb_1.y_max, aabb_2.y_max);
    if (window.x_max - window.x_min <= tolerance
            || window.y_max - window.y_min <= tolerance) {
        return false;
    }

    // Separated convex hulls.
    Point translation_opposite_hull = {-translation.x, -translation.y};
    if (separated_by_an_edge(polygon_2.convex_hull, polygon_1.convex_hull, translation)
            || separated_by_an_edge(polygon_1.convex_hull, polygon_2.convex_hull, translation_opposite_hull)) {
        return false;
    }

    if (boundary_enters(polygon_1, translation, polygon_2, window))
        return true;
    Point translation_opposite = {-translation.x, -translation.y};
    AxisAlignedBoundingBox window_2 = translate(window, translation_opposite);
    if (boundary_enters(polygon_2, translation_opposite, polygon_1, window_2))
        return true;

    // The boundaries don't enter the interiors: the interiors are either
    // disjoint or equal.
    if (locate_point(translate(polygon_1.interior_point, translation), polygon_2) == PointLocation::Inside)
        return true;
    if (locate_translated_point(polygon_2.interior_point, polygon_1, translation) == PointLocation::Inside)
        return true;
    return false;
}

std::vector<Pole> packingsolver::irregular::compute_poles(
        const Polygon& polygon,
        Counter maximum_number_of_poles)
{
    AreaDbl polygon_area = 0.0;
    ElementPos number_of_vertices = polygon.vertices.size();
    for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
        const Point& point_1 = polygon.vertices[vertex_pos];
        const Point& point_2 = polygon.vertices[(vertex_pos + 1) % number_of_vertices];
        polygon_area += point_1.x * point_2.y - point_2.x * point_1.y;
    }
    polygon_area = std::abs(polygon_area) / 2;
    LengthDbl size = (std::max)(
            polygon.aabb.x_max - polygon.aabb.x_min,
            polygon.aabb.y_max - polygon.aabb.y_min);

    std::vector<Pole> poles;
    AreaDbl poles_area = 0.0;
    while ((Counter)poles.size() < maximum_number_of_poles) {
        Pole pole = compute_next_pole(polygon, poles);
        // Stop if the remaining region is too thin; keep at least one pole.
        if (!poles.empty() && pole.radius <= 1e-3 * size)
            break;
        poles.push_back(pole);
        poles_area += M_PI * pole.radius * pole.radius;
        double coverage = poles_area / polygon_area;
        if (coverage > 0.9 && poles.size() >= 8)
            break;
        if (coverage > 0.8 && poles.size() >= 16)
            break;
    }
    return poles;
}
