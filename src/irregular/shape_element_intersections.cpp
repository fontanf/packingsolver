#include "irregular/shape_element_intersections.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{

// Helper function to check if a point lies on an arc
bool is_point_on_arc(
        const Point& p,
        const ShapeElement& arc)
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
        const ShapeElement& line2,
        bool strict)
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
        const ShapeElement& arc,
        bool strict)
{
    // Transform line to quadratic equation coefficients
    LengthDbl dx = line.end.x - line.start.x;
    LengthDbl dy = line.end.y - line.start.y;
    LengthDbl length = distance(line.start, line.end);

    if (equal(length, 0.0))
        return {};

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
    if (discriminant < 0)
        return {};

    std::vector<Point> intersections;
    if (equal(discriminant, 0.0)) {
        // One intersection
        LengthDbl t = -b / (2 * a);
        if (t < 0 || t > length)
            return {};

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
        const ShapeElement& arc2,
        bool strict)
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

}

std::vector<Point> irregular::compute_intersections(
        const ShapeElement& element_1,
        const ShapeElement& element_2,
        bool strict)
{
    if (element_1.type == ShapeElementType::LineSegment
            && element_2.type == ShapeElementType::LineSegment) {
        // Line segment - Line segment intersection
        return compute_line_line_intersections(element_1, element_2, strict);
    } else if (element_1.type == ShapeElementType::LineSegment
            && element_2.type == ShapeElementType::CircularArc) {
        // Line segment - Circular arc intersection
        return compute_line_arc_intersections(element_1, element_2, strict);
    } else if (element_1.type == ShapeElementType::CircularArc
            && element_2.type == ShapeElementType::LineSegment) {
        return compute_line_arc_intersections(element_2, element_1, strict);
    } else if (element_1.type == ShapeElementType::CircularArc
            && element_2.type == ShapeElementType::CircularArc) {
        // Circular arc - Circular arc intersection
        return compute_arc_arc_intersections(element_1, element_2, strict);
    }

    throw std::invalid_argument("irregular::compute_intersections: Invalid element types");
    return {};
}
