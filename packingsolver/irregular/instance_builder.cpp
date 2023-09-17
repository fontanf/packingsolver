#include "packingsolver/irregular/instance_builder.hpp"

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
    if (copies_min > copies) {
        throw std::runtime_error(
                "'irregular::InstanceBuilder::add_bin_type'"
                " requires 'copies_min <= copies'.");
    }

    BinType bin_type;
    bin_type.id = instance_.bin_types_.size();
    bin_type.shape = shape;
    bin_type.area = shape.compute_area();
    bin_type.x_min = std::min(bin_type.x_min, shape.compute_x_min());
    bin_type.x_max = std::max(bin_type.x_max, shape.compute_x_max());
    bin_type.y_min = std::min(bin_type.y_min, shape.compute_y_min());
    bin_type.y_max = std::max(bin_type.y_max, shape.compute_y_max());
    bin_type.cost = (cost == -1)? bin_type.area: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return bin_type.id;
}

void InstanceBuilder::add_defect(
        BinTypeId bin_type_id,
        DefectTypeId type,
        const Shape& shape,
        const std::vector<Shape>& holes)
{
    Defect defect;
    defect.id = instance_.bin_types_[bin_type_id].defects.size();
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
    item_type.id = instance_.item_types_.size();
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
    instance_.item_types_.push_back(item_type);
    return item_type.id;
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
        } else if (json_item["type"] == "polygon") {
            Shape shape;
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
        // Update item_profit_.
        instance_.item_profit_ += item_type.profit;
        // Update item_area_.
        instance_.item_area_ += item_type.area;
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
    }

    // Compute bin type attributes.
    instance_.bin_area_ = 0;
    AreaDbl previous_bins_area = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bins_area_sum_.
        instance_.bin_area_ += bin_type.copies * bin_type.area;
        // Update previous_bins_area_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_area_.push_back(previous_bins_area);
            previous_bins_area += bin_type.area;
        }
        // Update number_of_defects_.
        instance_.number_of_defects_ += bin_type.defects.size();
    }

    return std::move(instance_);
}
