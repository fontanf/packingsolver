#include "irregular/shape_element_intersections.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

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
    if (denom == 0.0) {
        if (strict)
            return {};

        // If they are colinear, check if they are aligned.
        LengthDbl denom_2 = (x1 - x2) * (y3 - y1) - (y1 - y2) * (x3 - x1);
        if (denom_2 != 0.0)
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

    // Edge cases.
    if (strict) {
        if (!strictly_greater(t, 0)
                || !strictly_lesser(t, 1)
                || !strictly_greater(u, 0)
                || !strictly_lesser(u, 1)) {
            // No intersection.
            return {};
        }
    } else {
        if (strictly_lesser(t, 0)
                || strictly_greater(t, 1)
                || strictly_lesser(u, 0)
                || strictly_greater(u, 1)) {
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
        }
    }

    // Standard intersection.
    LengthDbl xp = x1 + t * (x2 - x1);
    LengthDbl yp = y1 + t * (y2 - y1);
    return {{xp, yp}};
}

// Helper function to check if a point is a tangent point between line and arc
bool is_tangent_point(const ShapeElement& line, const ShapeElement& arc, const Point& point) {
    // A tangent point has its tangent vector perpendicular to the line direction
    // Calculate the tangent vector at the point on the circle
    Point tangent_vector;
    if (arc.anticlockwise) {
        tangent_vector = {
            -(point.y - arc.center.y),
            point.x - arc.center.x
        };
    } else {
        tangent_vector = {
            point.y - arc.center.y,
            -(point.x - arc.center.x)
        };
    }

    // Calculate line direction vector
    Point line_direction = {
        line.end.x - line.start.x,
        line.end.y - line.start.y
    };

    // Normalize vectors
    LengthDbl tangent_length = std::sqrt(tangent_vector.x * tangent_vector.x + tangent_vector.y * tangent_vector.y);
    tangent_vector.x /= tangent_length;
    tangent_vector.y /= tangent_length;

    LengthDbl line_length = std::sqrt(line_direction.x * line_direction.x + line_direction.y * line_direction.y);
    line_direction.x /= line_length;
    line_direction.y /= line_length;

    // Calculate dot product - if they're perpendicular, dot product will be close to 0
    LengthDbl dot_product = tangent_vector.x * line_direction.x + tangent_vector.y * line_direction.y;

    // If dot product is close to 0, they're perpendicular (tangent)
    return std::abs(dot_product) < 1e-10;
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
        // One intersection - this is a tangent point
        LengthDbl t = -b / (2 * a);
        if (t < 0 || t > length)
            return {};

        // Check if intersection is at endpoint of line segment
        if (equal(t, 0.0) || equal(t, length)) {
            // Skip endpoints in strict mode
            if (strict) {
                return {};
            }

            // Intersection is at an endpoint of line
            Point endpoint = equal(t, 0.0) ? line.start : line.end;
            if (arc.contains(endpoint)) {
                // Check if it's also an endpoint of the arc
                if (strict && (equal(distance(endpoint, arc.start), 0.0) || equal(distance(endpoint, arc.end), 0.0))) {
                    return {};
                }
                return {endpoint};
            }
            return {};
        }

        // Standard intersection point
        Point p = {
            line.start.x + t * dx,
            line.start.y + t * dy
        };

        if (arc.contains(p)) {
            // In strict mode, check if p is an endpoint of the arc
            if (strict && (equal(distance(p, arc.start), 0.0) || equal(distance(p, arc.end), 0.0))) {
                return {};
            }

            // In strict mode, also check if this is a tangent point (line touches circle)
            if (strict && equal(discriminant, 0.0)) {
                // For tangent points, we skip in strict mode
                return {};
            }

            intersections.push_back(p);
        }
    } else {
        // Two intersections
        LengthDbl t1 = (-b + std::sqrt(discriminant)) / (2 * a);
        LengthDbl t2 = (-b - std::sqrt(discriminant)) / (2 * a);

        // Check first intersection
        if (t1 >= 0 && t1 <= length) {
            // Handle endpoint cases
            if (equal(t1, 0.0) || equal(t1, length)) {
                if (!strict) {
                    Point endpoint = equal(t1, 0.0) ? line.start : line.end;
                    if (arc.contains(endpoint)) {
                        // Check if it's also an endpoint of the arc in strict mode
                        if (!(strict && (equal(distance(endpoint, arc.start), 0.0) || equal(distance(endpoint, arc.end), 0.0)))) {
                            intersections.push_back(endpoint);
                        }
                    }
                }
            } else {
                // Standard intersection
                Point p1 = {
                    line.start.x + t1 * dx,
                    line.start.y + t1 * dy
                };
                if (arc.contains(p1)) {
                    // In strict mode, check if p1 is an endpoint of the arc
                    if (!strict || (!equal(distance(p1, arc.start), 0.0) && !equal(distance(p1, arc.end), 0.0))) {
                        // Check if this is a tangent point
                        if (!strict || !is_tangent_point(line, arc, p1)) {
                            intersections.push_back(p1);
                        }
                    }
                }
            }
        }

        // Check second intersection
        if (t2 >= 0 && t2 <= length) {
            // Handle endpoint cases
            if (equal(t2, 0.0) || equal(t2, length)) {
                if (!strict) {
                    Point endpoint = equal(t2, 0.0) ? line.start : line.end;
                    if (arc.contains(endpoint)) {
                        // Check for duplicates
                        bool is_duplicate = false;
                        for (const auto& p : intersections) {
                            if (equal(distance(p, endpoint), 0.0)) {
                                is_duplicate = true;
                                break;
                            }
                        }
                        // Add if not a duplicate and not an arc endpoint in strict mode
                        if (!is_duplicate && !(strict && (equal(distance(endpoint, arc.start), 0.0) || equal(distance(endpoint, arc.end), 0.0)))) {
                            intersections.push_back(endpoint);
                        }
                    }
                }
            } else {
                // Standard intersection
                Point p2 = {
                    line.start.x + t2 * dx,
                    line.start.y + t2 * dy
                };
                if (arc.contains(p2)) {
                    // In strict mode, check if p2 is an endpoint of the arc
                    if (strict && (equal(distance(p2, arc.start), 0.0) || equal(distance(p2, arc.end), 0.0))) {
                        // Skip arc endpoints in strict mode
                    } else {
                        // Check if this is a tangent point
                        if (!strict || !is_tangent_point(line, arc, p2)) {
                            // Ensure we're not adding a duplicate point
                            bool is_duplicate = false;
                            for (const auto& p : intersections) {
                                if (equal(distance(p, p2), 0.0)) {
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

// Helper function to check if two arcs overlap
bool arcs_overlap(const ShapeElement& arc1, const ShapeElement& arc2) {
    // If centers or radii are different, they can only overlap at intersection points
    if (!equal(distance(arc1.center, arc2.center), 0.0) ||
        !equal(distance(arc1.center, arc1.start), distance(arc2.center, arc2.start))) {
        return false;
    }

    // Calculate angles for all endpoints
    auto calculate_angle = [](const Point& center, const Point& point) {
        return std::atan2(point.y - center.y, point.x - center.x);
    };

    LengthDbl start_angle_1 = calculate_angle(arc1.center, arc1.start);
    LengthDbl end_angle_1 = calculate_angle(arc1.center, arc1.end);
    LengthDbl start_angle_2 = calculate_angle(arc2.center, arc2.start);
    LengthDbl end_angle_2 = calculate_angle(arc2.center, arc2.end);

    // Normalize angles based on anticlockwise/clockwise
    if (!arc1.anticlockwise && end_angle_1 > start_angle_1) {
        end_angle_1 -= 2 * M_PI;
    } else if (arc1.anticlockwise && end_angle_1 < start_angle_1) {
        end_angle_1 += 2 * M_PI;
    }

    if (!arc2.anticlockwise && end_angle_2 > start_angle_2) {
        end_angle_2 -= 2 * M_PI;
    } else if (arc2.anticlockwise && end_angle_2 < start_angle_2) {
        end_angle_2 += 2 * M_PI;
    }

    // Check for overlap by seeing if any endpoint of one arc lies on the other arc
    if (!arc1.anticlockwise) {
        if ((start_angle_1 >= start_angle_2 && start_angle_1 <= end_angle_2) ||
            (end_angle_1 >= start_angle_2 && end_angle_1 <= end_angle_2) ||
            (start_angle_2 >= end_angle_1 && start_angle_2 <= start_angle_1) ||
            (end_angle_2 >= end_angle_1 && end_angle_2 <= start_angle_1)) {
            return true;
        }
    } else {
        if ((start_angle_1 <= start_angle_2 && start_angle_2 <= end_angle_1) ||
            (start_angle_1 <= end_angle_2 && end_angle_2 <= end_angle_1) ||
            (start_angle_2 <= start_angle_1 && start_angle_1 <= end_angle_2) ||
            (start_angle_2 <= end_angle_1 && end_angle_1 <= end_angle_2)) {
            return true;
        }
    }

    return false;
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

    // Check for arc overlap case (only if not strict)
    if (!strict && arcs_overlap(arc1, arc2)) {
        std::vector<Point> intersections;

        // For overlapping arcs, we return all endpoints that are contained in the other arc
        if (arc2.contains(arc1.start)) {
            intersections.push_back(arc1.start);
        }

        if (arc2.contains(arc1.end)) {
            // Check if the point is already in the intersections
            bool is_duplicate = false;
            for (const auto& p : intersections) {
                if (equal(distance(p, arc1.end), 0.0)) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                intersections.push_back(arc1.end);
            }
        }

        if (arc1.contains(arc2.start)) {
            // Check if the point is already in the intersections
            bool is_duplicate = false;
            for (const auto& p : intersections) {
                if (equal(distance(p, arc2.start), 0.0)) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                intersections.push_back(arc2.start);
            }
        }

        if (arc1.contains(arc2.end)) {
            // Check if the point is already in the intersections
            bool is_duplicate = false;
            for (const auto& p : intersections) {
                if (equal(distance(p, arc2.end), 0.0)) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                intersections.push_back(arc2.end);
            }
        }

        return intersections;
    }

    // Check for concentric arcs (same center)
    if (equal(d, 0.0)) {
        // If centers are the same and radii are the same, arcs may overlap
        if (equal(r1, r2) && !strict) {
            std::vector<Point> intersections;

            // For overlapping arcs, we need to return all endpoints that lie on both arcs
            if (arc2.contains(arc1.start)) {
                intersections.push_back(arc1.start);
            }

            if (arc2.contains(arc1.end)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc1.end), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc1.end);
                }
            }

            if (arc1.contains(arc2.start)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc2.start), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc2.start);
                }
            }

            if (arc1.contains(arc2.end)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc2.end), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc2.end);
                }
            }

            return intersections;
        }
        return {};
    }

    // Check if circles are too far apart or one inside another with no intersection
    if (d > r1 + r2 || d < std::abs(r1 - r2)) {
        return {};
    }

    // Handle overlapping arcs with same radius when centers are different but arc parts overlap
    if (equal(r1, r2) && !strict) {
        // Check if the arcs might overlap along their path (not just at intersection points)
        // For this to happen, the distance between centers must be less than 2*r
        if (d < 2 * r1) {
            std::vector<Point> intersections;

            // Calculate standard intersection points
            LengthDbl a = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
            LengthDbl h = std::sqrt(r1 * r1 - a * a);

            // Calculate the point P2 that lies on the line between the centers
            LengthDbl x3 = x1 + a * (x2 - x1) / d;
            LengthDbl y3 = y1 + a * (y2 - y1) / d;

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

            // Add intersection points if they lie on both arcs
            if (arc1.contains(p1) && arc2.contains(p1)) {
                intersections.push_back(p1);
            }

            if (arc1.contains(p2) && arc2.contains(p2)) {
                if (intersections.empty() || !equal(distance(p2, intersections[0]), 0.0)) {
                    intersections.push_back(p2);
                }
            }

            // Also check if any endpoints of one arc lie on the other arc
            if (arc2.contains(arc1.start)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc1.start), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc1.start);
                }
            }

            if (arc2.contains(arc1.end)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc1.end), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc1.end);
                }
            }

            if (arc1.contains(arc2.start)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc2.start), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc2.start);
                }
            }

            if (arc1.contains(arc2.end)) {
                // Check if the point is already in the intersections
                bool is_duplicate = false;
                for (const auto& p : intersections) {
                    if (equal(distance(p, arc2.end), 0.0)) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    intersections.push_back(arc2.end);
                }
            }

            return intersections;
        }
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
        if (arc1.contains(touching_point) && arc2.contains(touching_point)) {
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
    if (arc1.contains(p1) && arc2.contains(p1)) {
        // Check if p1 coincides with arc endpoints
        if (equal(distance(p1, arc1.start), 0.0)) {
            if (!strict) {
                intersections.push_back(arc1.start);
            }
        } else if (equal(distance(p1, arc1.end), 0.0)) {
            if (!strict) {
                intersections.push_back(arc1.end);
            }
        } else if (equal(distance(p1, arc2.start), 0.0)) {
            if (!strict) {
                intersections.push_back(arc2.start);
            }
        } else if (equal(distance(p1, arc2.end), 0.0)) {
            if (!strict) {
                intersections.push_back(arc2.end);
            }
        } else {
            intersections.push_back(p1);
        }
    }

    if (arc1.contains(p2) && arc2.contains(p2)) {
        // Check if p2 coincides with arc endpoints
        if (equal(distance(p2, arc1.start), 0.0)) {
            if (!strict && (intersections.empty() || !equal(distance(intersections[0], arc1.start), 0.0))) {
                intersections.push_back(arc1.start);
            }
        } else if (equal(distance(p2, arc1.end), 0.0)) {
            if (!strict && (intersections.empty() || !equal(distance(intersections[0], arc1.end), 0.0))) {
                intersections.push_back(arc1.end);
            }
        } else if (equal(distance(p2, arc2.start), 0.0)) {
            if (!strict && (intersections.empty() || !equal(distance(intersections[0], arc2.start), 0.0))) {
                intersections.push_back(arc2.start);
            }
        } else if (equal(distance(p2, arc2.end), 0.0)) {
            if (!strict && (intersections.empty() || !equal(distance(intersections[0], arc2.end), 0.0))) {
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
