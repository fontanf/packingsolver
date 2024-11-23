#include "packingsolver/irregular/instance.hpp"

#include <iostream>
#include <sstream>

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
    Angle a = std::atan2(
            cross_product(vector_2, vector_1),
            dot_product(vector_2, vector_1));
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
        return angle_radian(start - center, end - center) * r;
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
            + " center " + center.to_string();
    }
    }
    return "";
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
    if (!equal(elements[0].length(), elements[2].length()))
        return false;
    if (!equal(elements[1].length(), elements[3].length()))
        return false;
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
            LengthDbl radius = distance(elements.front().center, elements.front().start);
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

Shape packingsolver::irregular::build_polygon_shape(
        const std::vector<Point>& points)
{
    Shape shape;
    ElementPos pos_prev = points.size() - 1;
    for (ElementPos pos = 0; pos < (ElementPos)points.size(); ++pos) {
        ShapeElement element;
        element.type = ShapeElementType::LineSegment;
        element.start = points[pos_prev];
        element.end = points[pos];
        shape.elements.push_back(element);
        pos_prev = pos;
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

std::string ItemShape::to_string(
        Counter indentation) const
{
    std::string s = "\n";
    std::string indent = std::string(indentation, ' ');
    s += indent + "- shape: " + shape.to_string(indentation + 2) + "\n";
    if (holes.size() == 1) {
        s += indent + "- holes: " + holes.front().to_string(indentation + 2) + "\n";
    } else if (holes.size() >= 2) {
        s += indent + "- holes\n";
        for (const Shape& hole: holes)
            s += indent + "  - " + hole.to_string(indentation + 4) + "\n";
    }
    s += "- quality rule: " + std::to_string(quality_rule);
    return s;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::string Defect::to_string(
        Counter indentation) const
{
    std::string indent = std::string(indentation, ' ');
    std::string s = "defect:\n";
    s += "- type: " + std::to_string(type) + "\n";
    s += "- shape: " + shape.to_string(indentation + 2);
    return s;
}

ShapeType ItemType::shape_type() const
{
    // Circle.
    if (shapes.size() == 1
            && shapes.front().shape.is_circle()
            && shapes.front().holes.empty())
        return ShapeType::Circle;
    // Square.
    if (shapes.size() == 1
            && shapes.front().shape.is_square()
            && shapes.front().holes.empty())
        return ShapeType::Square;
    // Rectangle.
    if (shapes.size() == 1
            && shapes.front().shape.is_rectangle()
            && shapes.front().holes.empty())
        return ShapeType::Rectangle;
    // Polygon.
    if (shapes.size() == 1
            && shapes.front().shape.is_polygon()
            && shapes.front().holes.empty())
        return ShapeType::Polygon;
    // MultiPolygon.
    bool is_multi_polygon = true;
    for (const ItemShape& item_shape: shapes)
        if (!item_shape.shape.is_polygon()
                || !item_shape.holes.empty())
            is_multi_polygon = false;
    if (is_multi_polygon)
        return ShapeType::MultiPolygon;
    // PolygonWithHoles.
    if (shapes.size() == 1) {
        bool is_polygon_with_holes = true;
        if (!shapes.front().shape.is_polygon())
            is_polygon_with_holes = false;
        for (const Shape& hole: shapes.front().holes)
            if (!hole.is_polygon())
                is_polygon_with_holes = false;
        if (is_polygon_with_holes)
            return ShapeType::PolygonWithHoles;
    }
    // MultiPolygonWithHoles.
    bool is_multi_polygon_with_holes = true;
    for (const ItemShape& item_shape: shapes) {
        if (!item_shape.shape.is_polygon())
            is_multi_polygon_with_holes = false;
        for (const Shape& hole: item_shape.holes)
            if (!hole.is_polygon())
                is_multi_polygon_with_holes = false;
    }
    if (is_multi_polygon_with_holes)
        return ShapeType::MultiPolygonWithHoles;
    // GeneralShape.
    return ShapeType::GeneralShape;
}

std::pair<Point, Point> ItemType::compute_min_max(
        Angle angle,
        bool mirror) const
{
    LengthDbl x_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
    for (const ItemShape& item_shape: shapes) {
        auto points = item_shape.shape.compute_min_max(angle, mirror);
        x_min = std::min(x_min, points.first.x);
        x_max = std::max(x_max, points.second.x);
        y_min = std::min(y_min, points.first.y);
        y_max = std::max(y_max, points.second.y);
    }
    return {{x_min, y_min}, {x_max, y_max}};
}

bool ItemType::has_full_continuous_rotations() const
{
    if (allowed_rotations.size() != 1)
        return false;
    return (allowed_rotations[0].first == 0
            && allowed_rotations[0].second >= 2 * M_PI);
}

bool ItemType::has_only_discrete_rotations() const
{
    for (const auto& angles: allowed_rotations)
        if (angles.first != angles.second)
            return false;
    return true;
}

std::string ItemType::to_string(
        Counter indentation) const
{
    std::string indent = std::string(indentation, ' ');
    std::string s = "item type:\n";
    if (shapes.size() == 1) {
        s += indent +  "- shape: " + shapes.front().to_string(indentation + 2) + "\n";
    } else if (shapes.size() >= 2) {
        s += indent +  "- shape\n";
        for (const ItemShape& shape: shapes)
            s += indent + "  - " + shape.to_string(indentation + 4) + "\n";
    }
    s += indent + "- angles\n";
    for (AnglePos angle_pos = 0;
            angle_pos < (AnglePos)allowed_rotations.size();
            ++angle_pos) {
        s += indent + "  - "
            + std::to_string(allowed_rotations[angle_pos].first)
            + " -> " + std::to_string(allowed_rotations[angle_pos].second)
            + "\n";
    }
    s += indent + "- profit " + std::to_string(profit) + "\n";
    s += indent + "- copies " + std::to_string(copies);
    return s;
}

void ItemType::write_svg(
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

    // Loop through trapezoids of the trapezoid set.
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)shapes.size();
            ++item_shape_pos) {
        const auto& item_shape = shapes[item_shape_pos];
        file << "<g>" << std::endl;
        file << to_svg(item_shape.shape, item_shape.holes, factor);
        //file << "<text x=\"" << std::to_string(x * factor)
        //    << "\" y=\"" << std::to_string(-y * factor)
        //    << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">"
        //    << std::to_string(item_shape_pos)
        //    << "</text>" << std::endl;
        file << "</g>" << std::endl;
    }

    file << "</svg>" << std::endl;
}

std::string BinType::to_string(
        Counter indentation) const
{
    std::string indent = std::string(indentation, ' ');
    std::string s = "bin type:\n";
    s += indent + "- shape: " + shape.to_string(indentation + 2) + "\n";
    s += indent + "- copies: " + std::to_string(copies) + "\n";
    s += indent + "- defects:" + "\n";
    for (const Defect& defect: defects)
        s += indent + "  - " + defect.to_string(indentation + 4);
    return s;
}

void BinType::write_svg(
        const std::string& file_path) const
{
    if (file_path.empty())
        return;
    std::ofstream file{file_path};
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + file_path + "\".");
    }

    LengthDbl width = (x_max - x_min);
    LengthDbl height = (y_max - y_min);

    double factor = compute_svg_factor(width);
    while (width * factor > 1000)
        factor /= 10;
    while (width * factor < 100)
        factor *= 10;

    std::string s = "<svg viewBox=\""
        + std::to_string(x_min * factor)
        + " " + std::to_string(-y_min * factor - height * factor)
        + " " + std::to_string(width * factor)
        + " " + std::to_string(height * factor)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    // Loop through trapezoids of the trapezoid set.
    file << "<g>" << std::endl;
    file << to_svg(shape, {}, factor);
    //file << "<text x=\"" << std::to_string(x * factor)
    //    << "\" y=\"" << std::to_string(-y * factor)
    //    << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">"
    //    << std::to_string(item_shape_pos)
    //    << "</text>" << std::endl;
    file << "</g>" << std::endl;

    for (const Defect& defect: defects) {
        file << "<g>" << std::endl;
        file << to_svg(defect.shape, defect.holes, factor);
        file << "</g>" << std::endl;
    }

    file << "</svg>" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool Instance::can_contain(
        QualityRule quality_rule,
        DefectTypeId type) const
{
    if (type < 0 || type > (QualityRule)parameters_.quality_rules[quality_rule].size())
        return false;
    return parameters_.quality_rules[quality_rule][type];
}

std::ostream& Instance::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Objective:                    " << objective() << std::endl
            << "Number of item types:         " << number_of_item_types() << std::endl
            << "Number of items:              " << number_of_items() << std::endl
            << "Number of bin types:          " << number_of_bin_types() << std::endl
            << "Number of bins:               " << number_of_bins() << std::endl
            << "Number of defects:            " << number_of_defects() << std::endl
            << "Number of rectangular items:  " << number_of_rectangular_items_ << std::endl
            << "Number of circular items:     " << number_of_circular_items_ << std::endl
            << "Item-bin minimum spacing:     " << parameters().item_bin_minimum_spacing << std::endl
            << "Item-item minimum spacing:    " << parameters().item_item_minimum_spacing << std::endl
            << "Item area:                    " << item_area() << std::endl
            << "Smallest item area:           " << smallest_item_area() << std::endl
            << "Largest item area:            " << largest_item_area() << std::endl
            << "Maximum item copies:          " << maximum_item_copies() << std::endl
            << "Bin area:                     " << bin_area() << std::endl
            << "Maximum bin cost:             " << maximum_bin_cost() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Area"
            << std::setw(12) << "Cost"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Copies min"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "----"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
            << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            os
                << std::setw(12) << bin_type_id
                << std::setw(12) << bin_type.area
                << std::setw(12) << bin_type.cost
                << std::setw(12) << bin_type.copies
                << std::setw(12) << bin_type.copies_min
                << std::endl;
        }

        if (number_of_defects() > 0) {
            os
                << std::endl
                << std::setw(12) << "Bin type"
                << std::setw(12) << "Defect"
                << std::setw(12) << "Type"
                << std::endl
                << std::setw(12) << "--------"
                << std::setw(12) << "------"
                << std::setw(12) << "----"
                << std::endl;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < number_of_bin_types();
                    ++bin_type_id) {
                const BinType& bin_type = this->bin_type(bin_type_id);
                for (DefectId defect_id = 0;
                        defect_id < (DefectId)bin_type.defects.size();
                        ++defect_id) {
                    const Defect& defect = bin_type.defects[defect_id];
                    os
                        << std::setw(12) << bin_type_id
                        << std::setw(12) << defect_id
                        << std::setw(12) << defect.type
                        << std::endl;
                }
            }
        }

        os
            << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Shape type"
            << std::setw(12) << "Area"
            << std::setw(12) << "Profit"
            << std::setw(12) << "Copies"
            << std::setw(12) << "# shapes"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "----------"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "--------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << shape2str(item_type.shape_type())
                << std::setw(12) << item_type.area
                << std::setw(12) << item_type.profit
                << std::setw(12) << item_type.copies
                << std::setw(12) << item_type.shapes.size()
                << std::endl;
        }
    }

    if (verbosity_level >= 3) {
        // Item shapes
        os
            << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Shape"
            << std::setw(12) << "Q. rule"
            << std::setw(12) << "# holes"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-----"
            << std::setw(12) << "-------"
            << std::setw(12) << "-------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            for (Counter shape_pos = 0;
                    shape_pos < (Counter)item_type.shapes.size();
                    ++shape_pos) {
                const ItemShape& item_shape = item_type.shapes[shape_pos];
                os
                    << std::setw(12) << item_type_id
                    << std::setw(12) << shape_pos
                    << std::setw(12) << item_shape.quality_rule
                    << std::setw(12) << item_shape.holes.size()
                    << std::endl;
            }
        }

        // Elements.
        os
            << std::endl
            << std::setw(8) << "Object"
            << std::setw(8) << "Shape"
            << std::setw(8) << "Hole"
            << std::setw(8) << "Element"
            << std::setw(10) << "XS"
            << std::setw(10) << "YS"
            << std::setw(10) << "XE"
            << std::setw(10) << "YE"
            << std::setw(10) << "XC"
            << std::setw(10) << "YC"
            << std::setw(10) << "ACW"
            << std::endl
            << std::setw(8) << "------"
            << std::setw(8) << "-----"
            << std::setw(8) << "----"
            << std::setw(8) << "-------"
            << std::setw(10) << "--"
            << std::setw(10) << "--"
            << std::setw(10) << "--"
            << std::setw(10) << "--"
            << std::setw(10) << "--"
            << std::setw(10) << "--"
            << std::setw(10) << "---"
            << std::endl;
        // Bins.
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            for (Counter element_pos = 0;
                    element_pos < (Counter)bin_type.shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = bin_type.shape.elements[element_pos];
                os
                    << std::setw(2) << "B"
                    << std::setw(6) << bin_type_id
                    << std::setw(8) << -1
                    << std::setw(8) << -1
                    << std::setw(6) << element_pos
                    << std::setw(2) << element2char(element.type)
                    << std::setw(10) << element.start.x
                    << std::setw(10) << element.start.y
                    << std::setw(10) << element.end.x
                    << std::setw(10) << element.end.y
                    << std::setw(10) << element.center.x
                    << std::setw(10) << element.center.y
                    << std::setw(10) << element.anticlockwise
                    << std::endl;
            }
            // Defects.
            for (DefectId k = 0; k < (DefectId)bin_type.defects.size(); ++k) {
                const Defect& defect = bin_type.defects[k];
                for (Counter element_pos = 0;
                        element_pos < (Counter)defect.shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = bin_type.shape.elements[element_pos];
                    os
                        << std::setw(2) << "B"
                        << std::setw(6) << bin_type_id
                        << std::setw(8) << k
                        << std::setw(8) << -1
                        << std::setw(6) << element_pos
                        << std::setw(2) << element2char(element.type)
                        << std::setw(10) << element.start.x
                        << std::setw(10) << element.start.y
                        << std::setw(10) << element.end.x
                        << std::setw(10) << element.end.y
                        << std::setw(10) << element.center.x
                        << std::setw(10) << element.center.y
                        << std::setw(10) << element.anticlockwise
                        << std::endl;
                }
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)defect.holes.size();
                        ++hole_pos) {
                    const Shape& hole = defect.holes[hole_pos];
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        os
                            << std::setw(2) << "I"
                            << std::setw(6) << bin_type_id
                            << std::setw(8) << k
                            << std::setw(8) << hole_pos
                            << std::setw(6) << element_pos
                            << std::setw(2) << element2char(element.type)
                            << std::setw(10) << element.start.x
                            << std::setw(10) << element.start.y
                            << std::setw(10) << element.end.x
                            << std::setw(10) << element.end.y
                            << std::setw(10) << element.center.x
                            << std::setw(10) << element.center.y
                            << std::setw(10) << element.anticlockwise
                            << std::endl;
                    }
                }
            }
        }
        // Items.
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            for (Counter shape_pos = 0;
                    shape_pos < (Counter)item_type.shapes.size();
                    ++shape_pos) {
                const ItemShape& item_shape = item_type.shapes[shape_pos];
                for (Counter element_pos = 0;
                        element_pos < (Counter)item_shape.shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = item_shape.shape.elements[element_pos];
                    os
                        << std::setw(2) << "I"
                        << std::setw(6) << item_type_id
                        << std::setw(8) << shape_pos
                        << std::setw(8) << -1
                        << std::setw(6) << element_pos
                        << std::setw(2) << element2char(element.type)
                        << std::setw(10) << element.start.x
                        << std::setw(10) << element.start.y
                        << std::setw(10) << element.end.x
                        << std::setw(10) << element.end.y
                        << std::setw(10) << element.center.x
                        << std::setw(10) << element.center.y
                        << std::setw(10) << element.anticlockwise
                        << std::endl;
                }
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)item_shape.holes.size();
                        ++hole_pos) {
                    const Shape& hole = item_shape.holes[hole_pos];
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        os
                            << std::setw(2) << "I"
                            << std::setw(6) << item_type_id
                            << std::setw(8) << shape_pos
                            << std::setw(8) << hole_pos
                            << std::setw(6) << element_pos
                            << std::setw(2) << element2char(element.type)
                            << std::setw(10) << element.start.x
                            << std::setw(10) << element.start.y
                            << std::setw(10) << element.end.x
                            << std::setw(10) << element.end.y
                            << std::setw(10) << element.center.x
                            << std::setw(10) << element.center.y
                            << std::setw(10) << element.anticlockwise
                            << std::endl;
                    }
                }
            }
        }
    }

    return os;
}

void Instance::write(
        const std::string& instance_path) const
{
    std::ofstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + instance_path + "\".");
    }

    nlohmann::json json;

    std::stringstream objective_ss;
    objective_ss << objective();
    json["objective"] = objective_ss.str();

    // Export bins.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        json["bin_types"][bin_type_id]["cost"] = bin_type.cost;
        json["bin_types"][bin_type_id]["copies"] = bin_type.copies;
        json["bin_types"][bin_type_id]["copies_min"] = bin_type.copies_min;
        json["bin_types"][bin_type_id]["type"] = "general";
        for (ElementPos element_pos = 0;
                element_pos < (ElementPos)bin_type.shape.elements.size();
                ++element_pos) {
            const ShapeElement& element = bin_type.shape.elements[element_pos];
            json["bin_types"][bin_type_id]["elements"][element_pos]["type"] = element2str(element.type);
            json["bin_types"][bin_type_id]["elements"][element_pos]["start"]["x"] = element.start.x;
            json["bin_types"][bin_type_id]["elements"][element_pos]["start"]["y"] = element.start.y;
            json["bin_types"][bin_type_id]["elements"][element_pos]["end"]["x"] = element.end.x;
            json["bin_types"][bin_type_id]["elements"][element_pos]["end"]["y"] = element.end.y;
            if (element.type == ShapeElementType::CircularArc) {
                json["bin_types"][bin_type_id]["elements"][element_pos]["center"]["x"] = element.center.x;
                json["bin_types"][bin_type_id]["elements"][element_pos]["center"]["y"] = element.center.y;
                json["bin_types"][bin_type_id]["elements"][element_pos]["anticlockwise"] = element.anticlockwise;
            }
        }
        // Bin defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            json["bin_types"][bin_type_id]["defects"][defect_id]["type"] = "general";
            for (Counter element_pos = 0;
                    element_pos < (Counter)defect.shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = defect.shape.elements[element_pos];
                json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["type"] = element2str(element.type);
                json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["start"]["x"] = element.start.x;
                json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["start"]["y"] = element.start.y;
                json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["end"]["x"] = element.end.x;
                json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["end"]["y"] = element.end.y;
                if (element.type == ShapeElementType::CircularArc) {
                    json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["center"]["x"] = element.center.x;
                    json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["center"]["y"] = element.center.y;
                    json["bin_types"][bin_type_id]["defects"][defect_id]["elements"][element_pos]["anticlockwise"] = element.anticlockwise;
                }
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)defect.holes.size();
                        ++hole_pos) {
                    const Shape& hole = defect.holes[hole_pos];
                    json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["type"] = "general";
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["type"] = element2str(element.type);
                        json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["start"]["x"] = element.start.x;
                        json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["start"]["y"] = element.start.y;
                        json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["end"]["x"] = element.end.x;
                        json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["end"]["y"] = element.end.y;
                        if (element.type == ShapeElementType::CircularArc) {
                            json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["center"]["x"] = element.center.x;
                            json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["center"]["y"] = element.center.y;
                            json["bin_types"][bin_type_id]["defects"][defect_id]["holes"][hole_pos]["elements"][element_pos]["anticlockwise"] = element.anticlockwise;
                        }
                    }
                }
            }
        }
    }

    // Export items.
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        json["item_types"][item_type_id]["profit"] = item_type.profit;
        json["item_types"][item_type_id]["copies"] = item_type.copies;
        for (AnglePos angle_pos = 0;
                angle_pos < (AnglePos)item_type.allowed_rotations.size();
                ++angle_pos) {
            json["item_types"][item_type_id]["allowed_rotations"][angle_pos]["start"] = item_type.allowed_rotations[angle_pos].first;
            json["item_types"][item_type_id]["allowed_rotations"][angle_pos]["end"] = item_type.allowed_rotations[angle_pos].second;
        }

        for (Counter item_shape_pos = 0;
                item_shape_pos < (Counter)item_type.shapes.size();
                ++item_shape_pos) {
            const ItemShape& item_shape = item_type.shapes[item_shape_pos];
            json["item_types"][item_type_id]["shapes"][item_shape_pos]["type"] = "general";
            for (Counter element_pos = 0;
                    element_pos < (Counter)item_shape.shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = item_shape.shape.elements[element_pos];
                json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["type"] = element2str(element.type);
                json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["start"]["x"] = element.start.x;
                json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["start"]["y"] = element.start.y;
                json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["end"]["x"] = element.end.x;
                json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["end"]["y"] = element.end.y;
                if (element.type == ShapeElementType::CircularArc) {
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["center"]["x"] = element.center.x;
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["center"]["y"] = element.center.y;
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["elements"][element_pos]["anticlockwise"] = element.anticlockwise;
                }
            }
            for (Counter hole_pos = 0;
                    hole_pos < (Counter)item_shape.holes.size();
                    ++hole_pos) {
                const Shape& hole = item_shape.holes[hole_pos];
                json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["type"] = "general";
                for (Counter element_pos = 0;
                        element_pos < (Counter)hole.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = hole.elements[element_pos];
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["type"] = element2str(element.type);
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["start"]["x"] = element.start.x;
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["start"]["y"] = element.start.y;
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["end"]["x"] = element.end.x;
                    json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["end"]["y"] = element.end.y;
                    if (element.type == ShapeElementType::CircularArc) {
                        json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["center"]["x"] = element.center.x;
                        json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["center"]["y"] = element.center.y;
                        json["item_types"][item_type_id]["shapes"][item_shape_pos]["holes"][hole_pos]["elements"][element_pos]["anticlockwise"] = element.anticlockwise;
                    }
                }
            }
        }
    }

    file << std::setw(4) << json << std::endl;
}
