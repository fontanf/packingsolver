#include "irregular/utils.hpp"

#include "shape/elements_intersections.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////

namespace
{

LengthDbl arc_radius(const ShapeElement& arc)
{
    return distance(arc.start, arc.center);
}

std::pair<ShapeElement, ShapeElement> split_arc_at_midpoint(const ShapeElement& arc)
{
    Point midpoint = arc.middle();
    ShapeElement first_half = build_circular_arc(arc.start, midpoint, arc.center, arc.orientation);
    ShapeElement second_half = build_circular_arc(midpoint, arc.end, arc.center, arc.orientation);
    return std::make_pair(first_half, second_half);
}

/**
 * Build the CCW triangle T for a circular segment.
 *
 * T is formed by the chord p1-p2 and the tangent lines to the circle at p1
 * and p2; its apex is where those tangents meet (on the arc side of the chord).
 */
Shape build_circular_segment_triangle(
        const Point& p1,
        const Point& p2,
        const Point& circle_center)
{
    // CCW-perpendicular of the radius vector = tangent direction at that point.
    Point radius_1 = p1 - circle_center;
    Point radius_2 = p2 - circle_center;
    Point tangent_dir_1 = {-radius_1.y, radius_1.x};
    Point tangent_dir_2 = {-radius_2.y, radius_2.x};

    std::pair<bool, Point> intersection = compute_line_intersection(
            p1, p1 + tangent_dir_1,
            p2, p2 + tangent_dir_2);
    // intersection.first is always true here: arcs >= semicircle are rejected
    // before this function is called.
    Point apex = intersection.second;

    // Choose vertex order that gives a CCW triangle.
    LengthDbl signed_area_twice = (p2.x - p1.x) * (apex.y - p1.y)
                                - (p2.y - p1.y) * (apex.x - p1.x);
    if (signed_area_twice > 0) {
        return build_triangle(p1, p2, apex);
    } else {
        return build_triangle(p1, apex, p2);
    }
}

/**
 * Return true if the chord p1-p2 properly crosses any element of the boundary
 * other than the arc at arc_idx and its two neighbours (which share endpoints
 * with the chord).
 */
bool chord_intersects_boundary(
        const std::vector<ShapeElement>& elements,
        Counter arc_idx,
        const Point& p1,
        const Point& p2)
{
    Counter element_count = (Counter)elements.size();
    Counter prev_idx = (arc_idx + element_count - 1) % element_count;
    Counter next_idx = (arc_idx + 1) % element_count;

    ShapeElement chord = build_line_segment(p1, p2);

    for (Counter element_idx = 0; element_idx < element_count; ++element_idx) {
        if (element_idx == arc_idx
                || element_idx == prev_idx
                || element_idx == next_idx) {
            continue;
        }
        ShapeElementIntersectionsOutput result = compute_intersections(chord, elements[element_idx]);
        if (!result.proper_intersections.empty() || !result.overlapping_parts.empty())
            return true;
    }
    return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

BasicShapesDecomposition packingsolver::irregular::decompose_into_basic_shapes(
        const Shape& shape)
{
    BasicShapesDecomposition output;

    std::vector<ShapeElement> current_elements = shape.elements;

    bool found_arc = true;
    while (found_arc) {
        found_arc = false;
        for (Counter element_idx = 0;
                element_idx < (Counter)current_elements.size();
                ++element_idx) {
            const ShapeElement& element = current_elements[element_idx];
            if (element.type != ShapeElementType::CircularArc)
                continue;
            if (element.orientation != ShapeElementOrientation::Anticlockwise)
                continue;

            found_arc = true;

            // Check whether the arc subtends >= 180°; if so the tangent lines
            // at the endpoints are parallel and no finite apex exists.
            Point radius_start = element.start - element.center;
            Point radius_end = element.end - element.center;
            LengthDbl arc_angle = angle_radian(radius_start, radius_end);

            bool needs_subdivision = (arc_angle >= M_PI - 1e-6)
                    || chord_intersects_boundary(
                            current_elements,
                            element_idx,
                            element.start,
                            element.end);

            if (needs_subdivision) {
                std::pair<ShapeElement, ShapeElement> halves =
                        split_arc_at_midpoint(element);
                current_elements.erase(current_elements.begin() + element_idx);
                current_elements.insert(current_elements.begin() + element_idx, halves.second);
                current_elements.insert(current_elements.begin() + element_idx, halves.first);
                break;
            }

            // Cut the circular segment and replace the arc with its chord.
            BasicShape segment_basic_shape;
            segment_basic_shape.type = BasicShapeType::CircularSegment;
            segment_basic_shape.circle_center = element.center;
            segment_basic_shape.circle_radius = arc_radius(element);
            segment_basic_shape.shape = build_circular_segment_triangle(
                    element.start, element.end, element.center);
            output.basic_shapes.push_back(segment_basic_shape);

            current_elements[element_idx] = build_line_segment(element.start, element.end);
            break;
        }
    }

    // Decompose the remaining pure polygon into convex parts.
    ShapeWithHoles polygon_with_holes;
    polygon_with_holes.shape.elements = current_elements;
    for (const Shape& convex_part: compute_convex_partition(polygon_with_holes)) {
        BasicShape polygon_basic_shape;
        polygon_basic_shape.type = BasicShapeType::ConvexPolygon;
        polygon_basic_shape.shape = convex_part;
        output.basic_shapes.push_back(polygon_basic_shape);
    }

    return output;
}
