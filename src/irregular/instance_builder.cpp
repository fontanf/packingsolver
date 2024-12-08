#include "packingsolver/irregular/instance_builder.hpp"

#include <sstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Parameters //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Bin types ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinTypeId InstanceBuilder::add_bin_type(
        const Shape& shape,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (copies != -1) {
        if (copies <= 0) {
            throw std::runtime_error(
                    "'irregular::InstanceBuilder::add_bin_type'"
                    " requires 'copies > 0' or 'copies == -1'.");
        }
        if (copies_min > copies) {
            throw std::runtime_error(
                    "'irregular::InstanceBuilder::add_bin_type'"
                    " requires 'copies_min <= copies' or 'copies == -1'.");
        }
    }

    BinType bin_type;
    bin_type.shape = shape;
    bin_type.area = shape.compute_area();
    auto points = shape.compute_min_max();
    bin_type.x_min = points.first.x;
    bin_type.x_max = points.second.x;
    bin_type.y_min = points.first.y;
    bin_type.y_max = points.second.y;
    bin_type.cost = (cost == -1)? bin_type.area: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return instance_.bin_types_.size() - 1;
}

void InstanceBuilder::add_defect(
        BinTypeId bin_type_id,
        DefectTypeId type,
        const Shape& shape,
        const std::vector<Shape>& holes)
{
    Defect defect;
    defect.type = type;
    defect.shape = shape;
    defect.holes = holes;
    instance_.bin_types_[bin_type_id].defects.push_back(defect);
}

void InstanceBuilder::add_bin_type(
        const BinType& bin_type,
        BinPos copies,
        BinPos copies_min)
{
    BinTypeId bin_type_id = add_bin_type(
            bin_type.shape,
            bin_type.cost,
            copies,
            copies_min);
    for (const Defect& defect: bin_type.defects) {
        add_defect(
                bin_type_id,
                defect.type,
                defect.shape,
                defect.holes);
    }
}

void InstanceBuilder::set_bin_types_infinite_copies()
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].copies = -1;
    }
}

void InstanceBuilder::set_bin_types_unweighted()
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].area;
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Item types //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemTypeId InstanceBuilder::add_item_type(
        const std::vector<ItemShape>& shapes,
        Profit profit,
        ItemPos copies,
        const std::vector<std::pair<Angle, Angle>>& allowed_rotations)
{
    ItemType item_type;
    item_type.shapes = shapes;
    item_type.allowed_rotations = (!allowed_rotations.empty())?
        allowed_rotations: std::vector<std::pair<Angle, Angle>>{{0, 0}};
    item_type.area = 0;
    for (const auto& item_shape: item_type.shapes) {
        item_type.area += item_shape.shape.compute_area();
        for (const Shape& hole: item_shape.holes)
            item_type.area -= hole.compute_area();
    }
    item_type.profit = (profit != -1)? profit: item_type.area;
    item_type.copies = copies;
    instance_.item_types_.push_back(item_type);
    return instance_.item_types_.size() - 1;
}

void InstanceBuilder::set_item_type_allow_mirroring(
        ItemTypeId item_type_id,
        bool allow_mirroring)
{
    instance_.item_types_[item_type_id].allow_mirroring = allow_mirroring;
}

void InstanceBuilder::add_item_type(
        const ItemType& item_type,
        Profit profit,
        ItemPos copies)
{
    add_item_type(
            item_type.shapes,
            profit,
            copies,
            item_type.allowed_rotations);
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].area;
    }
}

AreaDbl InstanceBuilder::compute_bin_types_area_max() const
{
    AreaDbl bin_types_area_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        bin_types_area_max = std::max(
                bin_types_area_max,
                instance_.bin_type(bin_type_id).area);
    }
    return bin_types_area_max;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Read from files ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

template <class basic_json>
Shape read_shape(basic_json& json_item)
{
    Shape shape;
    if (json_item["type"] == "circle") {
        ShapeElement element;
        element.type = ShapeElementType::CircularArc;
        element.center = {0.0, 0.0};
        element.start = {json_item["radius"], 0.0};
        element.end = element.start;
        shape.elements.push_back(element);
    } else if (json_item["type"] == "rectangle") {
        ShapeElement element_1;
        ShapeElement element_2;
        ShapeElement element_3;
        ShapeElement element_4;
        element_1.type = ShapeElementType::LineSegment;
        element_2.type = ShapeElementType::LineSegment;
        element_3.type = ShapeElementType::LineSegment;
        element_4.type = ShapeElementType::LineSegment;
        element_1.start = {0.0, 0.0};
        element_1.end = {json_item["width"], 0.0};
        element_2.start = {json_item["width"], 0.0};
        element_2.end = {json_item["width"], json_item["height"]};
        element_3.start = {json_item["width"], json_item["height"]};
        element_3.end = {0.0, json_item["height"]};
        element_4.start = {0.0, json_item["height"]};
        element_4.end = {0.0, 0.0};
        shape.elements.push_back(element_1);
        shape.elements.push_back(element_2);
        shape.elements.push_back(element_3);
        shape.elements.push_back(element_4);
    } else if (json_item["type"] == "polygon") {
        for (auto it = json_item["vertices"].begin();
                it != json_item["vertices"].end();
                ++it) {
            auto it_next = it + 1;
            if (it_next == json_item["vertices"].end())
                it_next = json_item["vertices"].begin();
            ShapeElement element;
            element.type = ShapeElementType::LineSegment;
            element.start = {(*it)["x"], (*it)["y"]};
            element.end = {(*it_next)["x"], (*it_next)["y"]};
            shape.elements.push_back(element);
        }
    } else if (json_item["type"] == "general") {
        for (auto it = json_item["elements"].begin();
                it != json_item["elements"].end();
                ++it) {
            auto json_element = *it;
            ShapeElement element;
            element.type = str2element(json_element["type"]);
            element.start.x = json_element["start"]["x"];
            element.start.y = json_element["start"]["y"];
            element.end.x = json_element["end"]["x"];
            element.end.y = json_element["end"]["y"];
            if (element.type == ShapeElementType::CircularArc) {
                element.center.x = json_element["center"]["x"];
                element.center.y = json_element["center"]["y"];
                element.anticlockwise = json_element["anticlockwise"];
            }
            shape.elements.push_back(element);
        }
    } else {
        throw std::invalid_argument("");
    }
    return shape;
}

}

void InstanceBuilder::read(
        std::string instance_path)
{
    std::ifstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + instance_path + "\".");
    }

    nlohmann ::json j;
    file >> j;

    if (j.contains("objective")) {
        std::stringstream objective_ss;
        objective_ss << std::string(j["objective"]);
        Objective objective;
        objective_ss >> objective;
        set_objective(objective);
    }

    // Read parameters.
    if (j.contains("parameters")) {
        auto json_parameters = j["parameters"];
        if (json_parameters.contains("item_item_minimum_spacing"))
            set_item_item_minimum_spacing(json_parameters["item_item_minimum_spacing"]);
        if (json_parameters.contains("item_bin_minimum_spacing"))
            set_item_bin_minimum_spacing(json_parameters["item_bin_minimum_spacing"]);
    }

    // Read bin types.
    for (const auto& json_item: j["bin_types"]) {
        Shape shape = read_shape(json_item);

        Profit cost = -1;
        if (json_item.contains("cost"))
            cost = json_item["cost"];

        BinPos copies = 1;
        if (json_item.contains("copies"))
            copies = json_item["copies"];

        BinPos copies_min = 0;
        if (json_item.contains("copies_min"))
            copies_min = json_item["copies_min"];

        add_bin_type(shape, cost, copies, copies_min);
    }

    // Read item types.
    for (const auto& json_item: j["item_types"]) {
        std::vector<ItemShape> item_shapes;
        if (json_item.contains("shapes")) {
            // Multiple item shape.
            for (auto it_shape = json_item["shapes"].begin();
                    it_shape != json_item["shapes"].end();
                    ++it_shape) {
                auto json_shape = *it_shape;

                ItemShape item_shape;
                item_shape.shape = read_shape(json_shape);

                // Read holes.
                if (json_shape.contains("holes")) {
                    for (auto it_hole = json_shape["holes"].begin();
                            it_hole != json_shape["holes"].end();
                            ++it_hole) {
                        auto json_hole = *it_hole;
                        Shape shape = read_shape(json_hole);
                        item_shape.holes.push_back(shape);
                    }
                }

                item_shapes.push_back(item_shape);
            }

        } else {
            // Single item shape.
            ItemShape item_shape;
            item_shape.shape = read_shape(json_item);

            // Read holes.
            if (json_item.contains("holes")) {
                for (auto it_hole = json_item["holes"].begin();
                        it_hole != json_item["holes"].end();
                        ++it_hole) {
                    auto json_hole = *it_hole;
                    Shape shape = read_shape(json_hole);
                    item_shape.holes.push_back(shape);
                }
            }

            item_shapes.push_back(item_shape);
        }

        Profit profit = -1;
        if (json_item.contains("profit"))
            profit = json_item["profit"];

        ItemPos copies = 1;
        if (json_item.contains("copies"))
            copies = json_item["copies"];

        // Read allowed rotations.
        std::vector<std::pair<Angle, Angle>> allowed_rotations;
        if (json_item.contains("allowed_rotations")) {
            for (auto it = json_item["allowed_rotations"].begin();
                    it != json_item["allowed_rotations"].end();
                    ++it) {
                auto json_angles = *it;
                allowed_rotations.push_back({json_angles["start"], json_angles["end"]});
            }
        }

        // Read allow_mirroring.
        bool allow_mirroring = false;
        if (json_item.contains("allow_mirroring"))
            allow_mirroring = json_item["allow_mirroring"];

        ItemTypeId item_type_id = add_item_type(
                item_shapes,
                profit,
                copies,
                allowed_rotations);
        set_item_type_allow_mirroring(
                item_type_id,
                allow_mirroring);
    }

}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Instance InstanceBuilder::build()
{
    // Compute item type attributes.
    AreaDbl bin_types_area_max = compute_bin_types_area_max();
    instance_.all_item_types_infinite_copies_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        if (item_type.shape_type() == ShapeType::Square
                || item_type.shape_type() == ShapeType::Rectangle) {
            instance_.number_of_rectangular_items_ += item_type.copies;
        }
        if (item_type.shape_type() == ShapeType::Circle) {
            instance_.number_of_circular_items_ += item_type.copies;
        }
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update item_area_.
        instance_.item_area_ += item_type.copies * item_type.area;
        // Update smallest_item_area_ and largest_item_area_.
        instance_.smallest_item_area_ = (std::min)(
                instance_.smallest_item_area_,
                item_type.area);
        instance_.largest_item_area_ = (std::max)(
                instance_.largest_item_area_,
                item_type.area);
        // Update max_efficiency_item_type_.
        if (instance_.max_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.max_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.max_efficiency_item_type_id_).area
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).area) {
            instance_.max_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_area_max - 1) / item_type.area + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update maximum_item_copies_.
        if (instance_.maximum_item_copies_ < item_type.copies)
            instance_.maximum_item_copies_ = item_type.copies;
    }

    // Compute bin type attributes.
    instance_.bin_area_ = 0;
    AreaDbl previous_bins_area = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_area_sum_.
        instance_.bin_area_ += bin_type.copies * bin_type.area;
        // Update previous_bins_area_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_area_.push_back(previous_bins_area);
            previous_bins_area += bin_type.area;
        }
        // Update maximum_bin_cost_.
        if (instance_.maximum_bin_cost_ < bin_type.cost)
            instance_.maximum_bin_cost_ = bin_type.cost;
        // Update number_of_defects_.
        instance_.number_of_defects_ += bin_type.defects.size();
    }

    if (instance_.objective() == Objective::OpenDimensionX
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "irregular::InstanceBuilder::build."
                " The instance has objective OpenDimensionX and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionX must contain exactly one bin.");
    }
    if (instance_.objective() == Objective::OpenDimensionY
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "irregular::InstanceBuilder::build."
                " The instance has objective OpenDimensionY and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionY must contain exactly one bin.");
    }

    return std::move(instance_);
}
