#include "packingsolver/irregular/shape.hpp"

#include <cmath>

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Point /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::string Point::to_string() const
{
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

Point irregular::operator+(
        const Point& point_1,
        const Point& point_2)
{
    return {point_1.x + point_2.x, point_1.y + point_2.y};
}

Point irregular::operator-(
        const Point& point_1,
        const Point& point_2)
{
    return {point_1.x - point_2.x, point_1.y - point_2.y};
}

LengthDbl irregular::norm(
        const Point& vector)
{
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

LengthDbl irregular::distance(
        const Point& point_1,
        const Point& point_2)
{
    return norm(point_2 - point_1);
}

LengthDbl irregular::dot_product(
        const Point& vector_1,
        const Point& vector_2)
{
    return vector_1.x * vector_2.x + vector_1.y * vector_2.y;
}

LengthDbl irregular::cross_product(
        const Point& vector_1,
        const Point& vector_2)
{
    return vector_1.x * vector_2.y - vector_2.x * vector_1.y;
}

Point& Point::shift(
        LengthDbl x,
        LengthDbl y)
{
    this->x += x;
    this->y += y;
    return *this;
}

Point Point::rotate(
        Angle angle) const
{
    if (equal(angle, 0.0)) {
        return *this;
    } else if (equal(angle, 180)) {
        Point point_out;
        point_out.x = -x;
        point_out.y = -y;
        return point_out;
    } else if (equal(angle, 90)) {
        Point point_out;
        point_out.x = -y;
        point_out.y = x;
        return point_out;
    } else if (equal(angle, 270)) {
        Point point_out;
        point_out.x = y;
        point_out.y = -x;
        return point_out;
    } else {
        Point point_out;
        angle = M_PI * angle / 180;
        point_out.x = std::cos(angle) * x - std::sin(angle) * y;
        point_out.y = std::sin(angle) * x + std::cos(angle) * y;
        return point_out;
    }
}

Point Point::rotate_radians(
        Angle angle) const
{
    Point point_out;
    point_out.x = std::cos(angle) * x - std::sin(angle) * y;
    point_out.y = std::sin(angle) * x + std::cos(angle) * y;
    return point_out;
}

Point Point::axial_symmetry_identity_line() const
{
    Point point_out;
    point_out.x = y;
    point_out.y = x;
    return point_out;
}

Point Point::axial_symmetry_y_axis() const
{
    Point point_out;
    point_out.x = -x;
    point_out.y = y;
    return point_out;
}

Point Point::axial_symmetry_x_axis() const
{
    Point point_out;
    point_out.x = x;
    point_out.y = -y;
    return point_out;
}

Angle irregular::angle_radian(
        const Point& vector)
{
    Angle a = std::atan2(vector.y, vector.x);
    if (a < 0)
        a += 2 * M_PI;
    return a;
}

Angle irregular::angle_radian(
        const Point& vector_1,
        const Point& vector_2)
{
    Angle a = atan2(vector_2.y, vector_2.x) - atan2(vector_1.y, vector_1.x);
    if (a < 0)
        a += 2 * M_PI;
    return a;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// ShapeElement /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

LengthDbl ShapeElement::length() const
{
    switch (type) {
    case ShapeElementType::LineSegment:
        return distance(this->start, this->end);
    case ShapeElementType::CircularArc:
        LengthDbl r = distance(center, start);
        if (anticlockwise) {
            return angle_radian(start - center, end - center) * r;
        } else {
            return angle_radian(end - center, start - center) * r;
        }
    }
    return -1;
}

bool ShapeElement::contains(const Point& point) const
{
    switch (type) {
    case ShapeElementType::LineSegment: {
        // Calculate the squared length of the line segment
        LengthDbl line_length_squared = std::pow(end.x - start.x, 2) + std::pow(end.y - start.y, 2);

        // If the line segment is actually a point
        if (equal(line_length_squared, 0.0)) {
            return equal(point.x, start.x) && equal(point.y, start.y);
        }

        // Calculate parameter t, representing the position of the point on the line segment (between 0 and 1 indicates on the segment)
        LengthDbl t = ((point.x - start.x) * (end.x - start.x) + (point.y - start.y) * (end.y - start.y)) / line_length_squared;

        // If t is outside the [0,1] range, the point is not on the line segment
        if (strictly_lesser(t, 0.0) || strictly_greater(t, 1.0)) {
            return false;
        }

        // Calculate the projection point
        Point projection;
        projection.x = start.x + t * (end.x - start.x);
        projection.y = start.y + t * (end.y - start.y);

        // Check if the distance to the projection point is small enough
        LengthDbl distance_squared = std::pow(point.x - projection.x, 2) + std::pow(point.y - projection.y, 2);
        return equal(distance_squared, 0.0);
    } case ShapeElementType::CircularArc: {
        // Check if point lies on circle
        if (!equal(distance(point, this->center), distance(this->start, this->center))) {
            return false;
        }

        // Calculate angles
        Angle point_angle = angle_radian(point - this->center);
        Angle start_angle = angle_radian(this->start - this->center);
        Angle end_angle = angle_radian(this->end - this->center);

        // Normalize angles
        if (this->anticlockwise) {
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
    }
    return -1;
}

std::string ShapeElement::to_string() const
{
    switch (type) {
    case ShapeElementType::LineSegment: {
        return "LineSegment start " + start.to_string()
            + " end " + end.to_string();
    } case ShapeElementType::CircularArc: {
        return "CircularArc start " + start.to_string()
            + " end " + end.to_string()
            + " center " + center.to_string()
            + ((anticlockwise)? " anticlockwise": " clockwise");
    }
    }
    return "";
}

nlohmann::json ShapeElement::to_json() const
{
    nlohmann::json json;
    json["type"] = element2str(type);
    json["start"]["x"] = start.x;
    json["start"]["y"] = start.y;
    json["end"]["x"] = end.x;
    json["end"]["y"] = end.y;
    if (type == ShapeElementType::CircularArc) {
        json["center"]["x"] = center.x;
        json["center"]["y"] = center.y;
        json["anticlockwise"] = anticlockwise;
    }
    return json;
}

ShapeElement ShapeElement::rotate(
        Angle angle) const
{
    ShapeElement element_out = *this;
    element_out.start = start.rotate(angle);
    element_out.end = end.rotate(angle);
    element_out.center = center.rotate(angle);
    return element_out;
}

ShapeElement ShapeElement::axial_symmetry_identity_line() const
{
    ShapeElement element_out = *this;
    element_out.start = end.axial_symmetry_identity_line();
    element_out.end = start.axial_symmetry_identity_line();
    element_out.center = center.axial_symmetry_identity_line();
    element_out.anticlockwise = !anticlockwise;
    return element_out;
}

ShapeElement ShapeElement::axial_symmetry_x_axis() const
{
    ShapeElement element_out = *this;
    element_out.start = end.axial_symmetry_x_axis();
    element_out.end = start.axial_symmetry_x_axis();
    element_out.center = center.axial_symmetry_x_axis();
    element_out.anticlockwise = !anticlockwise;
    return element_out;
}

ShapeElement ShapeElement::axial_symmetry_y_axis() const
{
    ShapeElement element_out = *this;
    element_out.start = end.axial_symmetry_y_axis();
    element_out.end = start.axial_symmetry_y_axis();
    element_out.center = center.axial_symmetry_y_axis();
    element_out.anticlockwise = !anticlockwise;
    return element_out;
}

ShapeElementType irregular::str2element(const std::string& str)
{
    if (str == "LineSegment"
            || str == "line_segment"
            || str == "L"
            || str == "l") {
        return ShapeElementType::LineSegment;
    } else if (str == "CircularArc"
            || str == "circular_arc"
            || str == "C"
            || str == "c") {
        return ShapeElementType::CircularArc;
    } else {
        throw std::invalid_argument("");
        return ShapeElementType::LineSegment;
    }
}

std::string irregular::element2str(ShapeElementType type)
{
    switch (type) {
    case ShapeElementType::LineSegment: {
        return "LineSegment";
    } case ShapeElementType::CircularArc: {
        return "CircularArc";
    }
    }
    return "";
}

char irregular::element2char(ShapeElementType type)
{
    switch (type) {
    case ShapeElementType::LineSegment: {
        return 'L';
    } case ShapeElementType::CircularArc: {
        return 'C';
    }
    }
    return ' ';
}

std::vector<ShapeElement> irregular::approximate_circular_arc_by_line_segments(
        const ShapeElement& circular_arc,
        ElementPos number_of_line_segments,
        bool outer)
{
    if (circular_arc.type != ShapeElementType::CircularArc) {
        throw std::runtime_error(
                "packingsolver::irregular::circular_arc_to_line_segments: "
                "input element must be of type CircularArc; "
                "circular_arc.type: " + element2str(circular_arc.type) + ".");
    }
    if (!outer && number_of_line_segments < 1) {
        throw std::runtime_error(
                "packingsolver::irregular::circular_arc_to_line_segments: "
                "at least 1 line segment is needed to inner approximate a circular arc; "
                "outer: " + std::to_string(outer) + "; "
                "number_of_line_segments: " + std::to_string(number_of_line_segments) + ".");
    }

    Angle angle = (circular_arc.anticlockwise)?
        angle_radian(
            circular_arc.start - circular_arc.center,
            circular_arc.end - circular_arc.center):
        angle_radian(
            circular_arc.end - circular_arc.center,
            circular_arc.start - circular_arc.center);
    if ((outer && circular_arc.anticlockwise)
            || (!outer && !circular_arc.anticlockwise)) {
        if (angle < M_PI && number_of_line_segments < 2) {
            throw std::runtime_error(
                    "packingsolver::irregular::circular_arc_to_line_segments: "
                    "at least 2 line segments are needed to approximate the circular arc; "
                    "circular_arc: " + circular_arc.to_string() + "; "
                    "outer: " + std::to_string(outer) + "; "
                    "angle: " + std::to_string(angle) + "; "
                    "number_of_line_segments: " + std::to_string(number_of_line_segments) + ".");
        } else if (angle >= M_PI && number_of_line_segments < 3) {
            throw std::runtime_error(
                    "packingsolver::irregular::circular_arc_to_line_segments: "
                    "at least 3 line segments are needed to approximate the circular arc; "
                    "circular_arc: " + circular_arc.to_string() + "; "
                    "outer: " + std::to_string(outer) + "; "
                    "angle: " + std::to_string(angle) + "; "
                    "number_of_line_segments: " + std::to_string(number_of_line_segments) + ".");
        }
    }

    std::vector<ShapeElement> line_segments;
    LengthDbl radius = distance(circular_arc.center, circular_arc.start);
    Point point_prev = circular_arc.start;
    Point point_circle_prev = circular_arc.start;
    for (ElementPos line_segment_id = 0;
            line_segment_id < number_of_line_segments - 1;
            ++line_segment_id) {
        Angle angle_cur = (angle * (line_segment_id + 1)) / (number_of_line_segments - 1);
        if (!circular_arc.anticlockwise)
            angle_cur *= -1;
        Point point_circle;
        point_circle.x = circular_arc.center.x
            + std::cos(angle_cur) * (circular_arc.start.x - circular_arc.center.x)
            - std::sin(angle_cur) * (circular_arc.start.y - circular_arc.center.y);
        point_circle.y = circular_arc.center.y
            + std::sin(angle_cur) * (circular_arc.start.x - circular_arc.center.x)
            + std::cos(angle_cur) * (circular_arc.start.y - circular_arc.center.y);
        Point point_cur;
        if ((outer && !circular_arc.anticlockwise) || (!outer && circular_arc.anticlockwise)) {
            point_cur = point_circle;
        } else {
            // https://en.wikipedia.org/wiki/Tangent_lines_to_circles#Cartesian_equation
            // https://en.wikipedia.org/wiki/Intersection_(geometry)#Two_lines
            // The tangent line of the circle at (x1, y1) has Cartesian equation
            // (x - x1)(x1 - xc) + (y - y1)(y1 - yc) = 0
            // (x1 - xc) * x + (y1 - yc) * y - x1 * (x1 - xc) - y1 * (y1 - yc) = 0
            // At (x2, y2)
            // (x2 - xc) * x + (y2 - yc) * y - x2 * (x2 - xc) - y2 * (y1 - yc) = 0
            LengthDbl a1 = (point_circle_prev.x - circular_arc.center.x);
            LengthDbl b1 = (point_circle_prev.y - circular_arc.center.y);
            LengthDbl c1 = point_circle_prev.x * (point_circle_prev.x - circular_arc.center.x)
                + point_circle_prev.y * (point_circle_prev.y - circular_arc.center.y);
            LengthDbl a2 = (point_circle.x - circular_arc.center.x);
            LengthDbl b2 = (point_circle.y - circular_arc.center.y);
            LengthDbl c2 = point_circle.x * (point_circle.x - circular_arc.center.x)
                + point_circle.y * (point_circle.y - circular_arc.center.y);
            point_cur.x = (c1 * b2 - c2 * b1) / (a1 * b2 - a2 * b1);
            point_cur.y = (a1 * c2 - a2 * c1) / (a1 * b2 - a2 * b1);
        }
        ShapeElement line_segment;
        line_segment.start = point_prev;
        line_segment.end = point_cur;
        line_segment.type = ShapeElementType::LineSegment;
        line_segments.push_back(line_segment);
        point_prev = point_cur;
        point_circle_prev = point_circle;
    }
    ShapeElement line_segment;
    line_segment.start = point_prev;
    line_segment.end = circular_arc.end;
    line_segment.type = ShapeElementType::LineSegment;
    line_segments.push_back(line_segment);
    return line_segments;
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Shape /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::string irregular::shape2str(ShapeType type)
{
    switch (type) {
    case ShapeType::Circle: {
        return "C";
    } case ShapeType::Square: {
        return "S";
    } case ShapeType::Rectangle: {
        return "R";
    } case ShapeType::Polygon: {
        return "P";
    } case ShapeType::PolygonWithHoles: {
        return "PH";
    } case ShapeType::MultiPolygon: {
        return "MP";
    } case ShapeType::MultiPolygonWithHoles: {
        return "MPH";
    } case ShapeType::GeneralShape: {
        return "G";
    }
    }
    return "";
}

bool Shape::is_circle() const
{
    return (elements.size() == 1
            && elements.front().type == ShapeElementType::CircularArc);
}

bool Shape::is_square() const
{
    if (elements.size() != 4)
        return false;
    auto it_prev = std::prev(elements.end());
    for (auto it = elements.begin(); it != elements.end(); ++it) {
        if (it->type != ShapeElementType::LineSegment)
            return false;
        // Check angle.
        Angle theta = angle_radian(it_prev->start - it_prev->end, it->end - it->start);
        if (!equal(theta, M_PI / 2))
            return false;
        // Check length.
        if (!equal(it->length(), elements[0].length()))
            return false;
        it_prev = it;
    }
    return true;
}

bool Shape::is_rectangle() const
{
    if (elements.size() != 4)
        return false;
    auto it_prev = std::prev(elements.end());
    for (auto it = elements.begin(); it != elements.end(); ++it) {
        if (it->type != ShapeElementType::LineSegment)
            return false;
        // Check angle.
        Angle theta = angle_radian(it_prev->start - it_prev->end, it->end - it->start);
        if (!equal(theta, M_PI / 2))
            return false;
        it_prev = it;
    }
    return true;
}

bool Shape::is_polygon() const
{
    for (auto it = elements.begin(); it != elements.end(); ++it)
        if (it->type != ShapeElementType::LineSegment)
            return false;
    return true;
}

AreaDbl Shape::compute_area() const
{
    AreaDbl area = 0.0;
    for (const ShapeElement& element: elements) {
        area += cross_product(element.start, element.end);
        // Handle circular arcs.
        if (element.type == ShapeElementType::CircularArc) {
            LengthDbl radius = distance(element.center, element.start);
            Angle theta = angle_radian(element.center - element.start, element.center - element.end);
            if (element.anticlockwise) {
                area += radius * radius * ((!(element.start == element.end))? theta: 2.0 * M_PI);
            } else {
                area -= radius * radius * ((!(element.start == element.end))? 2.0 * M_PI - theta: 2.0 * M_PI);
            }
        }
    }
    return area / 2;
}

std::pair<Point, Point> Shape::compute_min_max(
        Angle angle,
        bool mirror) const
{
    LengthDbl x_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
    for (const ShapeElement& element: elements) {
        Point point = (!mirror)?
            element.start.rotate(angle):
            element.start.axial_symmetry_y_axis().rotate(angle);
        x_min = std::min(x_min, point.x);
        x_max = std::max(x_max, point.x);
        y_min = std::min(y_min, point.y);
        y_max = std::max(y_max, point.y);

        if (element.type == ShapeElementType::CircularArc) {
            LengthDbl radius = distance(element.center, element.start);
            Angle starting_angle = irregular::angle_radian(element.start - element.center);
            Angle ending_angle = irregular::angle_radian(element.end - element.center);
            if (!element.anticlockwise)
                std::swap(starting_angle, ending_angle);
            if (starting_angle <= ending_angle) {
                if (starting_angle <= M_PI
                        && M_PI <= ending_angle) {
                    x_min = std::min(x_min, element.center.x - radius);
                }
                if (starting_angle == 0)
                    x_max = std::max(x_max, element.center.x + radius);
                if (starting_angle <= 3 * M_PI / 2
                        && 3 * M_PI / 2 <= ending_angle) {
                    y_min = std::min(y_min, element.center.y - radius);
                }
                if (starting_angle <= M_PI / 2
                        && M_PI / 2 <= ending_angle) {
                    y_max = std::max(y_max, element.center.y + radius);
                }
            } else {  // starting_angle > ending_angle
                if (starting_angle <= M_PI
                        || ending_angle <= M_PI) {
                    x_min = std::min(x_min, element.center.x - radius);
                }
                x_max = std::max(x_max, element.center.x + radius);
                if (starting_angle <= 3 * M_PI / 4
                        || ending_angle <= 3 * M_PI / 4) {
                    y_min = std::min(y_min, element.center.y - radius);
                }
                if (starting_angle <= M_PI / 2
                        || ending_angle <= M_PI / 2) {
                    y_max = std::max(y_max, element.center.y + radius);
                }
            }
        }
    }
    return {{x_min, y_min}, {x_max, y_max}};
}

bool Shape::contains(
        const Point& point,
        bool strict) const
{
    if (this->elements.empty())
        return false;

    // First check if the point lies on any boundary.
    for (const ShapeElement& element : this->elements)
        if (element.contains(point))
            return (strict)? false: true;

    // Then use the ray-casting algorithm to check if the point is inside
    int intersection_count = 0;
    for (const ShapeElement& element: this->elements) {
        if (element.type == ShapeElementType::LineSegment) {
            // Handle the special case of horizontal line segments
            if (equal(element.start.y, element.end.y)) {
                // Horizontal line segment: if the point's y coordinate equals the segment's y coordinate,
                // and the x coordinate is within the segment range, the point is on the segment
                if (equal(point.y, element.start.y)
                        && (!strictly_lesser(point.x, std::min(element.start.x, element.end.x)))
                        && (!strictly_greater(point.x, std::max(element.start.x, element.end.x)))) {
                    // Already checked in the previous section, no need to handle here
                    continue;
                }
            }

            // Standard ray-casting algorithm for line segment checking
            // Cast a ray to the right from the point, count intersections with segments
            bool cond1 = (strictly_greater(element.start.y, point.y) != strictly_greater(element.end.y, point.y));
            bool cond2 = strictly_lesser(
                    point.x,
                    (element.end.x - element.start.x) * (point.y - element.start.y)
                    / (element.end.y - element.start.y) + element.start.x);

            if (cond1 && cond2) {
                intersection_count++;
            }
        } else if (element.type == ShapeElementType::CircularArc) {
            // Circular arc checking is more complex
            LengthDbl dx = point.x - element.center.x;
            LengthDbl dy = point.y - element.center.y;
            LengthDbl distance = std::sqrt(dx * dx + dy * dy);

            LengthDbl radius = std::sqrt(
                std::pow(element.start.x - element.center.x, 2)
                + std::pow(element.start.y - element.center.y, 2));

            // If the point is inside the circle and to the left of the center, there may be intersections with a ray to the right
            if (strictly_lesser(distance, radius) && strictly_lesser(point.x, element.center.x)) {
                LengthDbl start_angle = angle_radian({element.start.x - element.center.x, element.start.y - element.center.y});
                LengthDbl end_angle = angle_radian({element.end.x - element.center.x, element.end.y - element.center.y});

                // Ensure angles are in the correct range
                if (element.anticlockwise && end_angle <= start_angle) {
                    end_angle += 2 * M_PI;
                } else if (!element.anticlockwise && start_angle <= end_angle) {
                    start_angle += 2 * M_PI;
                }

                // Calculate the point's line-of-sight angle (angle between the line from point to center and the horizontal)
                LengthDbl point_angle = angle_radian({dx, dy});
                if (strictly_lesser(point_angle, 0)) {
                    point_angle += 2 * M_PI;  // Adjust angle to [0, 2π)
                }

                // Calculate the intersection angle of the ray to the right with the circle
                LengthDbl ray_angle = 0; // Angle of ray to the right is 0

                // Check if the ray intersects the arc
                bool intersects_arc;
                if (element.anticlockwise) {
                    intersects_arc = (!strictly_lesser(ray_angle, start_angle) && !strictly_greater(ray_angle, end_angle));
                } else {
                    intersects_arc = (!strictly_greater(ray_angle, start_angle) && !strictly_lesser(ray_angle, end_angle));
                }

                if (intersects_arc) {
                    intersection_count++;
                }
            }
        }
    }

    // If the number of intersections is odd, the point is inside the shape
    return (intersection_count % 2 == 1);
}

Shape& Shape::shift(
        LengthDbl x,
        LengthDbl y)
{
    for (ShapeElement& element: elements) {
        element.start.shift(x, y);
        element.end.shift(x, y);
        element.center.shift(x, y);
    }
    return *this;
}

Shape Shape::rotate(Angle angle) const
{
    Shape shape;
    for (const ShapeElement& element: elements) {
        ShapeElement element_new = element.rotate(angle);
        shape.elements.push_back(element_new);
    }
    return shape;
}

Shape Shape::axial_symmetry_identity_line() const
{
    Shape shape;
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        ShapeElement element_new = it->axial_symmetry_identity_line();
        shape.elements.push_back(element_new);
    }
    return shape;
}

Shape Shape::axial_symmetry_x_axis() const
{
    Shape shape;
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        ShapeElement element_new = it->axial_symmetry_x_axis();
        shape.elements.push_back(element_new);
    }
    return shape;
}

Shape Shape::axial_symmetry_y_axis() const
{
    Shape shape;
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        ShapeElement element_new = it->axial_symmetry_y_axis();
        shape.elements.push_back(element_new);
    }
    return shape;
}

Shape Shape::reverse() const
{
    Shape shape;
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        const ShapeElement& element = *it;
        ShapeElement element_new;
        element_new.type = element.type;
        element_new.start.x = element.end.x;
        element_new.start.y = element.end.y;
        element_new.end.x = element.start.x;
        element_new.end.y = element.start.y;
        element_new.center.x = element.center.x;
        element_new.center.y = element.center.y;
        element_new.anticlockwise = !element.anticlockwise;
        shape.elements.push_back(element_new);
    }
    return shape;
}

std::pair<LengthDbl, LengthDbl> Shape::compute_width_and_length(
        Angle angle,
        bool mirror) const
{
    auto points = compute_min_max(angle, mirror);
    LengthDbl width = points.second.x - points.first.x;
    LengthDbl height = points.second.y - points.first.y;
    return {width, height};
}

bool Shape::check() const
{
    // Check that the shape is not empty.
    if (elements.empty())
        return false;
    // TODO
    return true;
}

std::string Shape::to_string(
        Counter indentation) const
{
    std::string s = "";
    std::string indent = std::string(indentation, ' ');
    if (is_circle()) {
        LengthDbl radius = distance(elements.front().center, elements.front().start);
        s += "circle (radius: " + std::to_string(radius) + ")";
    } else if (is_square()) {
        s += "square (side: " + std::to_string(elements.front().length()) + ")";
    } else if (is_rectangle()) {
        s += "rectangle"
            " (side 1: " + std::to_string(elements[0].length())
            + "; side 2: " + std::to_string(elements[1].length()) + ")";
    } else if (is_polygon()) {
        s += "polygon (# edges " + std::to_string(elements.size()) + ")\n";
        for (Counter pos = 0; pos < (Counter)elements.size(); ++pos)
            s += indent + elements[pos].to_string() + ((pos < (Counter)elements.size() - 1)? "\n": "");
    } else  {
        s += "shape (# elements " + std::to_string(elements.size()) + ")\n";
        for (Counter pos = 0; pos < (Counter)elements.size(); ++pos)
            s += indent + elements[pos].to_string() + ((pos < (Counter)elements.size() - 1)? "\n": "");
    }
    return s;
}

nlohmann::json Shape::to_json() const
{
    nlohmann::json json;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)elements.size();
            ++element_pos) {
        json[element_pos] = elements[element_pos].to_json();
    }
    return json;
}

std::string Shape::to_svg(double factor) const
{
    std::string s = "M";
    for (const ShapeElement& element: elements) {
        s += std::to_string(element.start.x * factor)
            + "," + std::to_string(-(element.start.y * factor));
        if (element.type == ShapeElementType::LineSegment) {
            s += "L";
        } else {
            throw std::invalid_argument("");
        }
    }
    s += std::to_string(elements.front().start.x * factor)
        + "," + std::to_string(-(elements.front().start.y * factor))
        + "Z";
    return s;
}

void Shape::write_svg(
        const std::string& file_path) const
{
    if (file_path.empty())
        return;
    std::ofstream file{file_path};
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + file_path + "\".");
    }
    auto mm = compute_min_max(0.0);

    LengthDbl width = (mm.second.x - mm.first.x);
    LengthDbl height = (mm.second.y - mm.first.y);

    double factor = compute_svg_factor(width);

    std::string s = "<svg viewBox=\""
        + std::to_string(mm.first.x * factor)
        + " " + std::to_string(-mm.first.y * factor - height * factor)
        + " " + std::to_string(width * factor)
        + " " + std::to_string(height * factor)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    file << "<path d=\"M";
    for (const ShapeElement& element: elements) {
        file << (element.start.x * factor)
            << "," << -(element.start.y * factor);
        if (element.type == ShapeElementType::LineSegment) {
            file << "L";
        } else {
            throw std::invalid_argument("");
        }
    }
    file << (elements.front().start.x * factor)
        << "," << -(elements.front().start.y * factor)
        << "Z\""
        << " stroke=\"black\""
        << " stroke-width=\"1\""
        << " fill=\"blue\""
        << " fill-opacity=\"0.2\""
        << "/>" << std::endl;

    file << "</svg>" << std::endl;
}

Shape packingsolver::irregular::build_shape(
        const std::vector<BuildShapeElement>& points,
        bool path)
{
    Shape shape;
    Point point_prev = {points.back().x, points.back().y};
    ShapeElementType type = ShapeElementType::LineSegment;
    bool anticlockwise = false;
    Point center = {0, 0};
    for (ElementPos pos = 0; pos < (ElementPos)points.size(); ++pos) {
        const BuildShapeElement& point = points[pos];
        if (point.type == 0) {
            ShapeElement element;
            element.type = type;
            element.start = point_prev;
            element.end = {points[pos].x, points[pos].y};
            element.center = center;
            element.anticlockwise = anticlockwise;
            if (!path || pos > 0)
                shape.elements.push_back(element);
            point_prev = element.end;
            anticlockwise = true;
            center = {0, 0};
            type = ShapeElementType::LineSegment;
        } else {
            anticlockwise = (point.type == 1);
            center = {points[pos].x, points[pos].y};
            type = ShapeElementType::CircularArc;
        }
    }
    return shape;
}

double irregular::compute_svg_factor(
        double width)
{
    double factor = 1;
    while (width * factor > 10000)
        factor /= 10;
    while (width * factor < 1000)
        factor *= 10;
    return factor;
}

std::string irregular::to_svg(
        const Shape& shape,
        const std::vector<Shape>& holes,
        double factor,
        const std::string& fill_color)
{
    std::string s = "<path d=\"" + shape.to_svg(factor);
    for (const Shape& hole: holes)
        s += hole.reverse().to_svg(factor);
    s += "\""
        " stroke=\"black\""
        " stroke-width=\"1\"";
    if (!fill_color.empty()) {
        s += " fill=\"" + fill_color + "\""
            " fill-opacity=\"0.2\"";
    }
    s += "/>\n";
    return s;
}

void irregular::write_svg(
        const Shape& shape,
        const std::vector<Shape>& holes,
        const std::string& file_path)
{
    if (file_path.empty())
        return;
    std::ofstream file{file_path};
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + file_path + "\".");
    }

    auto mm = shape.compute_min_max(0.0);
    LengthDbl width = (mm.second.x - mm.first.x);
    LengthDbl height = (mm.second.y - mm.first.y);

    double factor = compute_svg_factor(width);

    std::string s = "<svg viewBox=\""
        + std::to_string(mm.first.x * factor)
        + " " + std::to_string(-mm.first.y * factor - height * factor)
        + " " + std::to_string(width * factor)
        + " " + std::to_string(height * factor)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    file << "<g>" << std::endl;
    file << to_svg(shape, holes, factor);
    //file << "<text x=\"" << std::to_string(x * factor)
    //    << "\" y=\"" << std::to_string(-y * factor)
    //    << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">"
    //    << std::to_string(item_shape_pos)
    //    << "</text>" << std::endl;
    file << "</g>" << std::endl;

    file << "</svg>" << std::endl;
}

std::pair<bool, Shape> irregular::remove_redundant_vertices(
        const Shape& shape)
{
    bool found = false;
    Shape shape_new;

    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape.elements[element_pos];
        if (equal(element.start.x, element.end.x)
                && equal(element.start.y, element.end.y)) {
            found = true;
            continue;
        }
        shape_new.elements.push_back(element);
    }

    return {found, shape_new};
}

std::pair<bool, Shape> irregular::remove_aligned_vertices(
        const Shape& shape)
{
    if (shape.elements.size() <= 3)
        return {false, shape};

    bool found = false;
    Shape shape_new;

    ElementPos element_prev_pos = shape.elements.size() - 1;
    for (ElementPos element_cur_pos = 0;
            element_cur_pos < (ElementPos)shape.elements.size();
            ++element_cur_pos) {
        ElementPos element_next_pos = element_cur_pos + 1;
        const ShapeElement& element_prev = shape.elements[element_prev_pos];
        const ShapeElement& element = shape.elements[element_cur_pos];
        const ShapeElement& element_next = (element_next_pos < shape.elements.size())?
            shape.elements[element_next_pos]:
            shape_new.elements.front();
        bool useless = false;
        if (element.type == ShapeElementType::LineSegment
                && element_prev.type == ShapeElementType::LineSegment) {
            double x1 = element_prev.start.x;
            double y1 = element_prev.start.y;
            double x2 = element.start.x;
            double y2 = element.start.y;
            double x3 = element_next.start.x;
            double y3 = element_next.start.y;
            double v = x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2);
            //std::cout << "element_prev  " << element_prev_pos << " " << element_prev.to_string() << std::endl;
            //std::cout << "element       " << element_cur_pos << " " << element.to_string() << std::endl;
            //std::cout << "element_next  " << element_next_pos << " " << element_next.to_string() << std::endl;
            //std::cout << "v " << v << std::endl;
            if (equal(v, 0)
                    || (equal(element_prev.start.y, element.start.y)
                        && equal(element.start.y, element_next.start.y))
                    || (equal(element_prev.start.x, element.start.x)
                        && equal(element.start.x, element_next.start.x))) {
                //std::cout << "useless " << element.to_string() << std::endl;
                useless = true;
                found = true;
            }
        }
        if (!useless) {
            if (!shape_new.elements.empty())
                shape_new.elements.back().end = element.start;
            shape_new.elements.push_back(element);
            element_prev_pos = element_cur_pos;
        }
    }
    shape_new.elements.back().end = shape_new.elements.front().start;

    return {found, shape_new};
}

std::pair<bool, Shape> irregular::equalize_close_y(
        const Shape& shape)
{
    bool found = false;
    Shape shape_new;

    ElementPos element_prev_pos = shape.elements.size() - 1;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape.elements[element_pos];
        const ShapeElement& element_prev = shape.elements[element_prev_pos];
        shape_new.elements.push_back(element);
        if (equal(element.start.y, element_prev.start.y)
                && element.start.y != element_prev.start.y) {
            shape_new.elements.back().start.y = element_prev.start.y;
            found = true;
        }
        element_prev_pos = element_pos;
    }

    return {found, shape_new};
}

Shape irregular::clean_shape(
        const Shape& shape)
{
    Shape shape_new = shape;

    auto res = remove_redundant_vertices(shape_new);
    shape_new = res.second;

    for (;;) {
        bool found = false;

        {
            auto res = remove_aligned_vertices(shape_new);
            found |= res.first;
            shape_new = res.second;
        }

        {
            auto res = equalize_close_y(shape_new);
            found |= res.first;
            shape_new = res.second;
        }

        if (!found)
            break;
    }

    return shape_new;
}

bool irregular::operator==(
        const ShapeElement& element_1,
        const ShapeElement& element_2)
{
    if (element_1.type != element_2.type)
        return false;
    if (!(element_1.start == element_2.start))
        return false;
    if (!(element_1.end == element_2.end))
        return false;
    if (element_1.type == ShapeElementType::CircularArc) {
        if (!(element_1.center == element_2.center))
            return false;
        if (element_1.anticlockwise != element_2.anticlockwise)
            return false;
    }
    return true;
}

bool irregular::equal(
        const ShapeElement& element_1,
        const ShapeElement& element_2)
{
    if (element_1.type != element_2.type)
        return false;
    if (!equal(element_1.start, element_2.start))
        return false;
    if (!equal(element_1.end, element_2.end))
        return false;
    if (element_1.type == ShapeElementType::CircularArc) {
        if (!equal(element_1.center, element_2.center))
            return false;
        if (element_1.anticlockwise != element_2.anticlockwise)
            return false;
    }
    return true;
}

bool irregular::operator==(
        const Shape& shape_1,
        const Shape& shape_2)
{
    // First, check if both shapes have the same number of elements.
    if (shape_1.elements.size() != shape_2.elements.size())
        return false;

    ElementPos offset = -1;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_2.elements.size();
            ++element_pos) {
        if (shape_2.elements[element_pos] == shape_1.elements[0]) {
            offset = element_pos;
            break;
        }
    }
    if (offset == -1)
        return false;

    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_2.elements.size();
            ++element_pos) {
        ElementPos element_pos_2 = (element_pos + offset) % shape_2.elements.size();
        if (!(shape_1.elements[element_pos] == shape_2.elements[element_pos_2])) {
            return false;
        }
    }

    return true;
}

bool irregular::equal(
        const Shape& shape_1,
        const Shape& shape_2)
{
    // First, check if both shapes have the same number of elements.
    if (shape_1.elements.size() != shape_2.elements.size())
        return false;

    ElementPos offset = -1;
    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_2.elements.size();
            ++element_pos) {
        if (equal(shape_2.elements[element_pos], shape_1.elements[0])) {
            offset = element_pos;
            break;
        }
    }
    if (offset == -1)
        return false;

    for (ElementPos element_pos = 0;
            element_pos < (ElementPos)shape_2.elements.size();
            ++element_pos) {
        ElementPos element_pos_2 = (element_pos + offset) % shape_2.elements.size();
        if (!equal(shape_1.elements[element_pos], shape_2.elements[element_pos_2])) {
            return false;
        }
    }

    return true;
}
