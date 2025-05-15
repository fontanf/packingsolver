#include "packingsolver/irregular/instance.hpp"

#include <iostream>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

bool ItemShape::check() const
{
    if (!shape_orig.shape.check())
        return false;
    for (const Shape& hole: shape_orig.holes)
        if (!hole.check())
            return false;
    return true;
}

std::string ItemShape::to_string(
        Counter indentation) const
{
    std::string s = "\n";
    std::string indent = std::string(indentation, ' ');
    s += indent + "- shape: " + shape_orig.to_string(indentation + 2) + "\n";
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
    s += "- shape: " + shape_orig.to_string(indentation + 2);
    return s;
}

ShapeType ItemType::shape_type() const
{
    // Circle.
    if (shapes.size() == 1
            && shapes.front().shape_scaled.shape.is_circle()
            && shapes.front().shape_scaled.holes.empty())
        return ShapeType::Circle;
    // Square.
    if (shapes.size() == 1
            && shapes.front().shape_scaled.shape.is_square()
            && shapes.front().shape_scaled.holes.empty())
        return ShapeType::Square;
    // Rectangle.
    if (shapes.size() == 1
            && shapes.front().shape_scaled.shape.is_rectangle()
            && shapes.front().shape_scaled.holes.empty())
        return ShapeType::Rectangle;
    // Polygon.
    if (shapes.size() == 1
            && shapes.front().shape_scaled.shape.is_polygon()
            && shapes.front().shape_scaled.holes.empty())
        return ShapeType::Polygon;
    // MultiPolygon.
    bool is_multi_polygon = true;
    for (const ItemShape& item_shape: shapes)
        if (!item_shape.shape_scaled.shape.is_polygon()
                || !item_shape.shape_scaled.holes.empty())
            is_multi_polygon = false;
    if (is_multi_polygon)
        return ShapeType::MultiPolygon;
    // PolygonWithHoles.
    if (shapes.size() == 1) {
        bool is_polygon_with_holes = true;
        if (!shapes.front().shape_scaled.shape.is_polygon())
            is_polygon_with_holes = false;
        for (const Shape& hole: shapes.front().shape_scaled.holes)
            if (!hole.is_polygon())
                is_polygon_with_holes = false;
        if (is_polygon_with_holes)
            return ShapeType::PolygonWithHoles;
    }
    // MultiPolygonWithHoles.
    bool is_multi_polygon_with_holes = true;
    for (const ItemShape& item_shape: shapes) {
        if (!item_shape.shape_scaled.shape.is_polygon())
            is_multi_polygon_with_holes = false;
        for (const Shape& hole: item_shape.shape_scaled.holes)
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
        bool mirror,
        int type) const
{
    LengthDbl x_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
    for (const ItemShape& item_shape: shapes) {
        auto points =
            (type == 0)? item_shape.shape_orig.shape.compute_min_max(angle, mirror):
            (type == 1)? item_shape.shape_scaled.shape.compute_min_max(angle, mirror):
            item_shape.shape_inflated.shape.compute_min_max(angle, mirror);
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

    auto mm = compute_min_max(0.0, false, 2);
    LengthDbl width = (mm.second.x - mm.first.x);
    LengthDbl height = (mm.second.y - mm.first.y);

    std::string s = "<svg viewBox=\""
        + std::to_string(mm.first.x)
        + " " + std::to_string(-mm.first.y - height)
        + " " + std::to_string(width)
        + " " + std::to_string(height)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    // Loop through trapezoids of the trapezoid set.
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)shapes.size();
            ++item_shape_pos) {
        const auto& item_shape = shapes[item_shape_pos];
        file << "<g>" << std::endl;
        file << item_shape.shape_scaled.to_svg("blue");
        file << item_shape.shape_inflated.to_svg("red");
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
    s += indent + "- shape: " + shape_scaled.to_string(indentation + 2) + "\n";
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

    std::string s = "<svg viewBox=\""
        + std::to_string(x_min)
        + " " + std::to_string(-y_min - height)
        + " " + std::to_string(width)
        + " " + std::to_string(height)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    // Loop through trapezoids of the trapezoid set.
    file << "<g>" << std::endl;
    file << shape_scaled.to_svg();
    //file << "<text x=\"" << std::to_string(x * factor)
    //    << "\" y=\"" << std::to_string(-y * factor)
    //    << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">"
    //    << std::to_string(item_shape_pos)
    //    << "</text>" << std::endl;
    file << "</g>" << std::endl;

    for (const Defect& defect: defects) {
        file << "<g>" << std::endl;
        file << defect.shape_scaled.to_svg();
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
    if (quality_rule == -1)
        return false;
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
            << "Total item area:              " << item_area() << std::endl
            << "Smallest item area:           " << smallest_item_area() << std::endl
            << "Largest item area:            " << largest_item_area() << std::endl
            << "Total item profit:            " << item_profit() << std::endl
            << "Largest item profit:          " << largest_item_profit() << std::endl
            << "Largest item copies:          " << largest_item_copies() << std::endl
            << "Total bin area:               " << bin_area() << std::endl
            << "Largest bin cost:             " << largest_bin_cost() << std::endl
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
                << std::setw(12) << bin_type.area_orig
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
                << std::setw(12) << item_type.area_orig
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
                    << std::setw(12) << item_shape.shape_orig.holes.size()
                    << std::endl;
            }
        }

        // Elements.
        os
            << std::endl
            << std::setw(7) << "Object"
            << std::setw(5) << "Sha."
            << std::setw(5) << "Hole"
            << std::setw(7) << "Elt."
            << std::setw(12) << "XS"
            << std::setw(12) << "YS"
            << std::setw(12) << "XE"
            << std::setw(12) << "YE"
            << std::setw(12) << "XC"
            << std::setw(12) << "YC"
            << std::setw(2) << "O"
            << std::endl
            << std::setw(7) << "------"
            << std::setw(5) << "----"
            << std::setw(5) << "----"
            << std::setw(7) << "----"
            << std::setw(12) << "--"
            << std::setw(12) << "--"
            << std::setw(12) << "--"
            << std::setw(12) << "--"
            << std::setw(12) << "--"
            << std::setw(12) << "--"
            << std::setw(2) << "-"
            << std::endl;
        // Bins.
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            for (Counter element_pos = 0;
                    element_pos < (Counter)bin_type.shape_orig.elements.size();
                    ++element_pos) {
                const ShapeElement& element = bin_type.shape_orig.elements[element_pos];
                os
                    << std::setw(2) << "B"
                    << std::setw(5) << bin_type_id
                    << std::setw(5) << -1
                    << std::setw(5) << -1
                    << std::setw(5) << element_pos
                    << std::setw(2) << element2char(element.type)
                    << std::setw(12) << element.start.x
                    << std::setw(12) << element.start.y
                    << std::setw(12) << element.end.x
                    << std::setw(12) << element.end.y
                    << std::setw(12) << element.center.x
                    << std::setw(12) << element.center.y
                    << std::setw(2) << shape::orientation2char(element.orientation)
                    << std::endl;
            }
            // Defects.
            for (DefectId k = 0; k < (DefectId)bin_type.defects.size(); ++k) {
                const Defect& defect = bin_type.defects[k];
                for (Counter element_pos = 0;
                        element_pos < (Counter)defect.shape_orig.shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = defect.shape_orig.shape.elements[element_pos];
                    os
                        << std::setw(2) << "B"
                        << std::setw(5) << bin_type_id
                        << std::setw(5) << k
                        << std::setw(5) << -1
                        << std::setw(5) << element_pos
                        << std::setw(2) << element2char(element.type)
                        << std::setw(12) << element.start.x
                        << std::setw(12) << element.start.y
                        << std::setw(12) << element.end.x
                        << std::setw(12) << element.end.y
                        << std::setw(12) << element.center.x
                        << std::setw(12) << element.center.y
                        << std::setw(2) << shape::orientation2char(element.orientation)
                        << std::endl;
                }
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)defect.shape_orig.holes.size();
                        ++hole_pos) {
                    const Shape& hole = defect.shape_orig.holes[hole_pos];
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        os
                            << std::setw(2) << "I"
                            << std::setw(5) << bin_type_id
                            << std::setw(5) << k
                            << std::setw(5) << hole_pos
                            << std::setw(5) << element_pos
                            << std::setw(2) << element2char(element.type)
                            << std::setw(12) << element.start.x
                            << std::setw(12) << element.start.y
                            << std::setw(12) << element.end.x
                            << std::setw(12) << element.end.y
                            << std::setw(12) << element.center.x
                            << std::setw(12) << element.center.y
                            << std::setw(2) << shape::orientation2char(element.orientation)
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
                        element_pos < (Counter)item_shape.shape_orig.shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element = item_shape.shape_orig.shape.elements[element_pos];
                    os
                        << std::setw(2) << "I"
                        << std::setw(5) << item_type_id
                        << std::setw(5) << shape_pos
                        << std::setw(5) << -1
                        << std::setw(5) << element_pos
                        << std::setw(2) << element2char(element.type)
                        << std::setw(12) << element.start.x
                        << std::setw(12) << element.start.y
                        << std::setw(12) << element.end.x
                        << std::setw(12) << element.end.y
                        << std::setw(12) << element.center.x
                        << std::setw(12) << element.center.y
                        << std::setw(2) << shape::orientation2char(element.orientation)
                        << std::endl;
                }
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)item_shape.shape_orig.holes.size();
                        ++hole_pos) {
                    const Shape& hole = item_shape.shape_orig.holes[hole_pos];
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        os
                            << std::setw(2) << "I"
                            << std::setw(5) << item_type_id
                            << std::setw(5) << shape_pos
                            << std::setw(5) << hole_pos
                            << std::setw(5) << element_pos
                            << std::setw(2) << element2char(element.type)
                            << std::setw(12) << element.start.x
                            << std::setw(12) << element.start.y
                            << std::setw(12) << element.end.x
                            << std::setw(12) << element.end.y
                            << std::setw(12) << element.center.x
                            << std::setw(12) << element.center.y
                            << std::setw(2) << shape::orientation2char(element.orientation)
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
        json["bin_types"][bin_type_id] = bin_type.shape_orig.to_json();
        json["bin_types"][bin_type_id]["cost"] = bin_type.cost;
        json["bin_types"][bin_type_id]["copies"] = bin_type.copies;
        json["bin_types"][bin_type_id]["copies_min"] = bin_type.copies_min;
        // Bin defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            json["bin_types"][bin_type_id]["defects"][defect_id] = defect.shape_orig.to_json();
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
            json["item_types"][item_type_id]["shapes"][item_shape_pos] = item_shape.shape_orig.to_json();
        }
    }

    // Export parameters.
    json["parameters"]["item_item_minimum_spacing"] = parameters().item_item_minimum_spacing;
    json["parameters"]["item_bin_minimum_spacing"] = parameters().item_bin_minimum_spacing;

    file << std::setw(4) << json << std::endl;
}
