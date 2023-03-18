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

AreaDbl Instance::previous_bin_area(BinPos i_pos) const
{
    assert(i_pos < number_of_bins());
    const BinType& b = bin(i_pos);
    return b.previous_bin_area + bin(i_pos).area * (i_pos - b.previous_bin_copies);
}

ItemTypeId Instance::add_item_type(
        const std::vector<ItemShape>& shapes,
        Profit profit,
        ItemPos copies,
        const std::vector<std::pair<Angle, Angle>>& allowed_rotations)
{
    ItemType item_type;
    item_type.id = item_types_.size();
    item_type.shapes = shapes;
    item_type.allowed_rotations = allowed_rotations;
    item_type.x_min = +std::numeric_limits<LengthDbl>::infinity();
    item_type.x_max = -std::numeric_limits<LengthDbl>::infinity();
    item_type.y_min = +std::numeric_limits<LengthDbl>::infinity();
    item_type.y_max = -std::numeric_limits<LengthDbl>::infinity();
    item_type.area = 0;
    for (const auto& item_shape: item_type.shapes) {
        item_type.x_min = std::min(item_type.x_min, item_shape.shape.compute_x_min());
        item_type.x_max = std::max(item_type.x_max, item_shape.shape.compute_x_max());
        item_type.y_min = std::min(item_type.y_min, item_shape.shape.compute_y_min());
        item_type.y_max = std::max(item_type.y_max, item_shape.shape.compute_y_max());
        item_type.area += item_shape.shape.compute_area();
        for (const Shape& hole: item_shape.holes)
            item_type.area -= hole.compute_area();
    }
    item_type.profit = (profit != -1)? profit: item_type.area;
    item_type.copies = copies;

    item_types_.push_back(item_type);

    number_of_items_ += copies; // Update number_of_items_

    // Compute item area and profit
    item_area_ += item_type.copies * item_type.area;
    item_profit_ += item_type.copies * item_type.profit;
    if (max_efficiency_item_ == -1
            || (item_types_[max_efficiency_item_].profit * item_type.area
                < item_type.profit * item_types_[max_efficiency_item_].area))
        max_efficiency_item_ = item_type.id;

    return item_type.id;
}

BinTypeId Instance::add_bin_type(
        const Shape& shape,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (copies_min > copies) {
        throw std::runtime_error(
                "'irregular::Instance::add_bin_type'"
                " requires 'copies_min <= copies'.");
    }

    BinType bin_type;
    bin_type.id = bin_types_.size();
    bin_type.shape = shape;
    bin_type.area = shape.compute_area();
    bin_type.cost = (cost == -1)? bin_type.area: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    bin_type.previous_bin_area = (number_of_bins() == 0)? 0:
        bin_types_.back().previous_bin_area + bin_types_.back().area * bin_types_.back().copies;
    bin_type.previous_bin_copies = (number_of_bins() == 0)? 0:
        bin_types_.back().previous_bin_copies + bin_types_.back().copies;

    bin_types_.push_back(bin_type);
    for (ItemPos pos = 0; pos < copies; ++pos)
        bins_pos2type_.push_back(bin_type.id);
    bin_area_ += bin_type.copies * bin_type.area;
    packable_area_ += bin_types_.back().copies * bin_types_.back().area; // Update packable_area_;

    return bin_type.id;
}

void Instance::add_defect(
        BinTypeId i,
        DefectTypeId type,
        const Shape& shape,
        const std::vector<Shape>& holes)
{
    Defect defect;
    defect.id = bin_types_[i].defects.size();
    defect.type = type;
    defect.shape = shape;
    defect.holes = holes;
    bin_types_[i].defects.push_back(defect);

    // Update packable_area_ and defect_area_
    // TODO
}

void Instance::read(
        std::string instance_path)
{
    std::ifstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + instance_path + "\".");
    }

    nlohmann ::json j;
    file >> j;

    // Read bin types.
    for (const auto& json_item: j["bin_types"]) {
        Shape shape;
        if (json_item["type"] == "rectangle") {
            ShapeElement element_1;
            ShapeElement element_2;
            ShapeElement element_3;
            ShapeElement element_4;
            element_1.type = ShapeElementType::LineSegment;
            element_2.type = ShapeElementType::LineSegment;
            element_3.type = ShapeElementType::LineSegment;
            element_4.type = ShapeElementType::LineSegment;
            element_1.start = {0.0, 0.0};
            element_1.end = {json_item["length"], 0.0};
            element_2.start = {json_item["length"], 0.0};
            element_2.end = {json_item["length"], json_item["height"]};
            element_3.start = {json_item["length"], json_item["height"]};
            element_3.end = {0.0, json_item["height"]};
            element_4.start = {0.0, json_item["height"]};
            element_4.end = {0.0, 0.0};
            shape.elements.push_back(element_1);
            shape.elements.push_back(element_2);
            shape.elements.push_back(element_3);
            shape.elements.push_back(element_4);
        } else {

        }
        Profit cost = shape.compute_area();
        BinPos copies = 1;
        BinPos copies_min = 0;
        add_bin_type(shape, cost, copies, copies_min);
    }

    // Read item types.
    for (const auto& json_item: j["item_types"]) {
        std::vector<ItemShape> item_shapes;
        if (json_item["type"] == "circle") {
            Shape shape;
            ShapeElement element;
            element.type = ShapeElementType::CircularArc;
            element.center = {0.0, 0.0};
            element.start = {json_item["radius"], 0.0};
            element.end = element.start;
            shape.elements.push_back(element);
            ItemShape item_shape;
            item_shape.shape = shape;
            item_shapes.push_back(item_shape);
        } else {

        }
        Profit profit = -1;
        BinPos copies = 1;
        add_item_type(
                item_shapes,
                profit,
                copies,
                {});
    }

}

bool Instance::can_contain(QualityRule quality_rule, DefectTypeId type) const
{
    if (type < 0 || type > (QualityRule)quality_rules_[quality_rule].size())
        return false;
    return quality_rules_[quality_rule][type];
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
            << std::setw(12) << "BIN TYPE"
            << std::setw(12) << "AREA"
            << std::setw(12) << "COST"
            << std::setw(12) << "COPIES"
            << std::setw(12) << "COPIES_MIN"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "----"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
            << std::endl;
        for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
            os
                << std::setw(12) << i
                << std::setw(12) << bin_type(i).area
                << std::setw(12) << bin_type(i).cost
                << std::setw(12) << bin_type(i).copies
                << std::setw(12) << bin_type(i).copies_min
                << std::endl;
        }

        if (number_of_defects() > 0) {
            os
                << std::endl
                << std::setw(12) << "BIN TYPE"
                << std::setw(12) << "DEFECT"
                << std::setw(12) << "TYPE"
                << std::endl
                << std::setw(12) << "--------"
                << std::setw(12) << "------"
                << std::setw(12) << "----"
                << std::endl;
            for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
                for (const Defect& defect: bin_type(i).defects) {
                    os
                        << std::setw(12) << i
                        << std::setw(12) << defect.id
                        << std::setw(12) << defect.type
                        << std::endl;
                }
            }
        }

        os
            << std::endl
            << std::setw(12) << "ITEM TYPE"
            << std::setw(12) << "SHAPE TYPE"
            << std::setw(12) << "AREA"
            << std::setw(12) << "PROFIT"
            << std::setw(12) << "COPIES"
            << std::setw(12) << "# SHAPES"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "----------"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "--------"
            << std::endl;
        for (ItemTypeId j = 0; j < number_of_item_types(); ++j) {
            os
                << std::setw(12) << j
                << std::setw(12) << shape2str(item_type(j).shape_type())
                << std::setw(12) << item_type(j).area
                << std::setw(12) << item_type(j).profit
                << std::setw(12) << item_type(j).copies
                << std::setw(12) << item_type(j).shapes.size()
                << std::endl;
        }
    }

    if (verbose >= 3) {
        // Item shapes
        os
            << std::endl
            << std::setw(12) << "ITEM TYPE"
            << std::setw(12) << "SHAPE"
            << std::setw(12) << "Q. RULE"
            << std::setw(12) << "# HOLES"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-----"
            << std::setw(12) << "-------"
            << std::setw(12) << "-------"
            << std::endl;
        for (ItemTypeId j = 0; j < number_of_item_types(); ++j) {
            for (Counter shape_pos = 0; shape_pos < (Counter)item_type(j).shapes.size(); ++shape_pos) {
                const ItemShape& item_shape = item_type(j).shapes[shape_pos];
                os
                    << std::setw(12) << j
                    << std::setw(12) << shape_pos
                    << std::setw(12) << item_shape.quality_rule
                    << std::setw(12) << item_shape.holes.size()
                    << std::endl;
            }
        }

        // Elements.
        os
            << std::endl
            << std::setw(8) << "OBJECT"
            << std::setw(8) << "SHAPE"
            << std::setw(8) << "HOLE"
            << std::setw(8) << "ELEMENT"
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
        for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
            for (Counter element_pos = 0; element_pos < (Counter)bin_type(i).shape.elements.size(); ++element_pos) {
                const ShapeElement& element = bin_type(i).shape.elements[element_pos];
                os
                    << std::setw(2) << "B"
                    << std::setw(6) << i
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
            for (DefectId k = 0; k < (DefectId)bin_type(i).defects.size(); ++k) {
                const Defect& defect = bin_type(i).defects[k];
                for (Counter element_pos = 0; element_pos < (Counter)defect.shape.elements.size(); ++element_pos) {
                    const ShapeElement& element = bin_type(i).shape.elements[element_pos];
                    os
                        << std::setw(2) << "B"
                        << std::setw(6) << i
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
                for (Counter hole_pos = 0; hole_pos < (Counter)defect.holes.size(); ++hole_pos) {
                    const Shape& hole = defect.holes[hole_pos];
                    for (Counter element_pos = 0; element_pos < (Counter)hole.elements.size(); ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        os
                            << std::setw(2) << "I"
                            << std::setw(6) << i
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
        for (ItemTypeId j = 0; j < number_of_item_types(); ++j) {
            for (Counter shape_pos = 0; shape_pos < (Counter)item_type(j).shapes.size(); ++shape_pos) {
                const ItemShape& item_shape = item_type(j).shapes[shape_pos];
                for (Counter element_pos = 0; element_pos < (Counter)item_shape.shape.elements.size(); ++element_pos) {
                    const ShapeElement& element = item_shape.shape.elements[element_pos];
                    os
                        << std::setw(2) << "I"
                        << std::setw(6) << j
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
                for (Counter hole_pos = 0; hole_pos < (Counter)item_shape.holes.size(); ++hole_pos) {
                    const Shape& hole = item_shape.holes[hole_pos];
                    for (Counter element_pos = 0; element_pos < (Counter)hole.elements.size(); ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        os
                            << std::setw(2) << "I"
                            << std::setw(6) << j
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

