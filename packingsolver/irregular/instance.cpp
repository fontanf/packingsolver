#include "packingsolver/irregular/instance.hpp"

#include "packingsolver/irregular/solution.hpp"

#include <iostream>
#include <fstream>
#include <climits>

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

Angle irregular::angle(
        const Point& vector_1,
        const Point& vector_2)
{
    Angle a = std::atan2(
            cross_product(vector_1, vector_2),
            dot_product(vector_1, vector_2));
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
        return angle(start - center, end - center) * r;
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
        Angle theta = angle(it_prev->start - it_prev->end, it->end - it->start);
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
        Angle theta = angle(it_prev->start - it_prev->end, it->end - it->start);
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
            Angle theta = angle(element.center - element.start, element.center - element.end);
            if (element.anticlockwise) {
                area += radius * radius * ((!(element.start == element.end))? theta: 2.0 * M_PI);
            } else {
                area -= radius * radius * ((!(element.start == element.end))? 2.0 * M_PI - theta: 2.0 * M_PI);
            }
        }
    }
    return area / 2;
}

LengthDbl Shape::compute_x_min() const
{
    LengthDbl x_min = elements.front().start.x;
    for (const ShapeElement& element: elements)
        x_min = std::min(x_min, element.start.x);
    return x_min;
}

LengthDbl Shape::compute_x_max() const
{
    LengthDbl x_max = elements.front().start.x;
    for (const ShapeElement& element: elements)
        x_max = std::max(x_max, element.start.x);
    return x_max;
}

LengthDbl Shape::compute_y_min() const
{
    LengthDbl y_min = elements.front().start.y;
    for (const ShapeElement& element: elements)
        y_min = std::min(y_min, element.start.y);
    return y_min;
}

LengthDbl Shape::compute_y_max() const
{
    LengthDbl y_max = elements.front().start.y;
    for (const ShapeElement& element: elements)
        y_max = std::max(y_max, element.start.y);
    return y_max;
}

LengthDbl Shape::compute_length() const
{
    return compute_x_max() - compute_x_min();
}

LengthDbl Shape::compute_width() const
{
    return compute_y_max() - compute_y_min();
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
    std::string s = "defect: " + std::to_string(id) + "\n";
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

std::string ItemType::to_string(
        Counter indentation) const
{
    std::string indent = std::string(indentation, ' ');
    std::string s = "item type: " + std::to_string(id) + "\n";
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

std::string BinType::to_string(
        Counter indentation) const
{
    std::string indent = std::string(indentation, ' ');
    std::string s = "bin type: " + std::to_string(id) + "\n";
    s += indent + "- shape: " + shape.to_string(indentation + 2) + "\n";
    s += indent + "- copies: " + std::to_string(copies) + "\n";
    s += indent + "- defects:" + "\n";
    for (const Defect& defect: defects)
        s += indent + "  - " + defect.to_string(indentation + 4);
    return s;
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

std::ostream& Instance::print(
        std::ostream& os,
        int verbose) const
{
    if (verbose >= 1) {
        os
            << "Objective:                " << objective() << std::endl
            << "Number of item types:     " << number_of_item_types() << std::endl
            << "Number of items:          " << number_of_items() << std::endl
            << "Number of bin types:      " << number_of_bin_types() << std::endl
            << "Number of bins:           " << number_of_bins() << std::endl
            << "Number of defects:        " << number_of_defects() << std::endl
            << "Item area:                " << item_area() << std::endl
            << "Bin area:                 " << bin_area() << std::endl
            ;
    }

    if (verbose >= 2) {
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
                        << std::setw(12) << defect.id
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

    if (verbose >= 3) {
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

