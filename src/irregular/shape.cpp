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
        const std::vector<BuildShapeElement>& points)
{
    Shape shape;
    Point point_prev = {points.back().x, points.back().y};
    bool anticlockwise = false;
    Point center = {0, 0};
    for (ElementPos pos = 0; pos < (ElementPos)points.size(); ++pos) {
        const BuildShapeElement point = points[pos];
        if (point.type == 0) {
            ShapeElement element;
            element.type = ShapeElementType::LineSegment;
            element.start = point_prev;
            element.end = {points[pos].x, points[pos].y};
            shape.elements.push_back(element);
            point_prev = element.end;
        } else {
            anticlockwise = (point.type == 1);
            center = {points[pos].x, points[pos].y};
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

std::vector<Shape> packingsolver::irregular::borders(
        const Shape& shape)
{
    std::vector<Shape> res;

    auto mm = shape.compute_min_max();
    //std::cout << "mm.first.x " << mm.first.x
    //    << " mm.first.y " << mm.first.y
    //    << " mm.second.x " << mm.second.x
    //    << " mm.second.y " << mm.second.y
    //    << std::endl;

    Shape shape_border;
    ElementPos element_0_pos = 0;
    for (ElementPos element_pos = 0;
            element_pos < shape.elements.size();
            ++element_pos) {
        const ShapeElement& shape_element = shape.elements[element_pos];
        if (shape_element.start.x == mm.first.x) {
            element_0_pos = element_pos;
            break;
        }
    }
    //std::cout << "element_0_pos " << element_0_pos << std::endl;
    // 0: left; 1: bottom; 2: right; 3: top.
    const ShapeElement& element_0 = shape.elements[element_0_pos];
    int start_border = (element_0.start.y == mm.first.y)? 1: 0;
    LengthDbl start_coordinate = element_0.start.y;
    for (ElementPos element_pos = 0;
            element_pos < shape.elements.size();
            ++element_pos) {
        const ShapeElement& element = shape.elements[(element_0_pos + element_pos) % shape.elements.size()];
        //std::cout << "element_pos " << ((element_0_pos + element_pos) % shape.elements.size())
        //    << " / " << shape.elements.size()
        //    << ": " << element.to_string()
        //    << "; start_border: " << start_border
        //    << std::endl;
        shape_border.elements.push_back(element);
        bool close = false;
        if (start_border == 0) {
            if (equal(element.end.x, mm.first.x)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.y, mm.first.y)) {
                    start_border = 0;
                } else {
                    start_border = 1;
                }
            } else if (equal(element.end.y, mm.first.y)) {
                if (element.end.x != shape_border.elements[0].start.x) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.first.x, mm.first.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.x, mm.second.x)) {
                    start_border = 1;
                } else {
                    start_border = 2;
                }
            }
        } else if (start_border == 1) {
            if (equal(element.end.y, mm.first.y)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.x, mm.second.x)) {
                    start_border = 1;
                } else {
                    start_border = 2;
                }
            } else if (equal(element.end.x, mm.second.x)) {
                if (element.end.y != shape_border.elements[0].start.y) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.second.x, mm.first.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.y, mm.second.y)) {
                    start_border = 2;
                } else {
                    start_border = 3;
                }
            }
        } else if (start_border == 2) {
            if (equal(element.end.x, mm.second.x)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.y, mm.second.y)) {
                    start_border = 2;
                } else {
                    start_border = 3;
                }
            } else if (equal(element.end.y, mm.second.y)) {
                if (element.end.y != shape_border.elements[0].start.y) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.second.x, mm.second.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.x, mm.second.x)) {
                    start_border = 3;
                } else {
                    start_border = 0;
                }
            }
        } else if (start_border == 3) {
            if (equal(element.end.y, mm.second.y)) {
                ShapeElement new_element;
                new_element.type = ShapeElementType::LineSegment;
                new_element.start = element.end;
                new_element.end = shape_border.elements[0].start;
                shape_border.elements.push_back(new_element);
                close = true;
                if (!equal(element.end.x, mm.first.x)) {
                    start_border = 3;
                } else {
                    start_border = 0;
                }
            } else if (equal(element.end.x, mm.first.x)) {
                if (element.end.x != shape_border.elements[0].start.x) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {mm.first.x, mm.second.y};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                }
                close = true;
                if (!equal(element.end.y, mm.second.y)) {
                    start_border = 0;
                } else {
                    start_border = 1;
                }
            }
        }
        //std::cout << "shape_border " << shape_border.to_string(0) << std::endl;
        // New shape.
        if (close) {
            //std::cout << "close " << shape_border.to_string(0) << std::endl;
            if (shape_border.elements.size() >= 3)
                res.push_back(shape_border.reverse());
            shape_border.elements.clear();
        }
    }

    return res;
}
