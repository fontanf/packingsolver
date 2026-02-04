#include "packingsolver/irregular/instance_builder.hpp"

#include "shape/offset.hpp"
#include "shape/extract_borders.hpp"
#include "shape/clean.hpp"

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
    if (cost <= 0 && cost != -1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'cost' must be > 0 (or == -1); "
                "cost: " + std::to_string(cost) + ".");
    }
    if (copies_min < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'copies_min' must be >= 0; "
                "copies_min: " + std::to_string(copies_min) + ".");
    }
    if (copies != -1) {
        if (copies <= 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin 'copies' must be > 0 (or == -1); "
                    "copies: " + std::to_string(copies) + ".");
        }
        if (copies_min > copies) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin 'copies_min' must be <= 'copies'; "
                    "copies: " + std::to_string(copies) + "; "
                    "copies_min: " + std::to_string(copies_min) + ".");
        }
    }

    BinType bin_type;
    bin_type.shape_orig = shape;
    bin_type.area_orig = shape.compute_area();
    auto points = shape.compute_min_max();
    bin_type.x_min = points.first.x;
    bin_type.x_max = points.second.x;
    bin_type.y_min = points.first.y;
    bin_type.y_max = points.second.y;
    bin_type.cost = (cost == -1)? bin_type.area_orig: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return instance_.bin_types_.size() - 1;
}

DefectId InstanceBuilder::add_defect(
        BinTypeId bin_type_id,
        DefectTypeId type,
        const ShapeWithHoles& shape)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = this->instance_.bin_types_[bin_type_id];
    Defect defect;
    defect.type = type;
    defect.shape_orig = shape;
    bin_type.defects.push_back(defect);
    return bin_type.defects.size() - 1;
}

void InstanceBuilder::add_bin_type(
        const BinType& bin_type,
        BinPos copies,
        BinPos copies_min)
{
    BinTypeId bin_type_id = add_bin_type(
            bin_type.shape_orig,
            bin_type.cost,
            copies,
            copies_min);
    this->set_item_bin_minimum_spacing(
            bin_type_id,
            bin_type.item_bin_minimum_spacing);
    for (const Defect& defect: bin_type.defects) {
        DefectId defect_id = add_defect(
                bin_type_id,
                defect.type,
                defect.shape_orig);
        this->set_item_defect_minimum_spacing(
                bin_type_id,
                defect_id,
                defect.item_defect_minimum_spacing);
    }
}

void InstanceBuilder::set_item_bin_minimum_spacing(
        BinTypeId bin_type_id,
        LengthDbl item_bin_minimum_spacing)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = this->instance_.bin_types_[bin_type_id];
    bin_type.item_bin_minimum_spacing = item_bin_minimum_spacing;
}

void InstanceBuilder::set_item_defect_minimum_spacing(
        BinTypeId bin_type_id,
        DefectId defect_id,
        LengthDbl item_defect_minimum_spacing)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = this->instance_.bin_types_[bin_type_id];

    if (defect_id < 0 || defect_id >= bin_type.defects.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'defect_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "defect_id: " + std::to_string(defect_id) + "; "
                "bin_type.defects.size(): " + std::to_string(bin_type.defects.size()) + ".");
    }
    Defect& defect = bin_type.defects[defect_id];
    defect.item_defect_minimum_spacing = item_defect_minimum_spacing;
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
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].area_orig;
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
    if (shapes.empty()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'shapes' must be non-empty.");
    }
    if (copies <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'copies' must be > 0; "
                "copies: " + std::to_string(copies) + ".");
    }

    ItemType item_type;
    item_type.shapes = shapes;
    for (ItemShape& item_shape: item_type.shapes) {
        item_shape.shape_orig = shape::remove_redundant_vertices(item_shape.shape_orig).second;
        if (!item_shape.check()) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "invalid shape.");
        }
    }
    item_type.allowed_rotations = (!allowed_rotations.empty())?
        allowed_rotations: std::vector<std::pair<Angle, Angle>>{{0, 0}};
    item_type.area_orig = 0;
    item_type.area_scaled = 0;
    for (const auto& item_shape: item_type.shapes) {
        item_type.area_orig += item_shape.shape_orig.shape.compute_area();
        for (const Shape& hole: item_shape.shape_orig.holes)
            item_type.area_orig -= hole.compute_area();
        item_type.area_scaled += item_shape.shape_scaled.shape.compute_area();
        for (const Shape& hole: item_shape.shape_scaled.holes)
            item_type.area_scaled -= hole.compute_area();
    }
    item_type.profit = (profit != -1)? profit: item_type.area_orig;
    item_type.copies = copies;
    instance_.item_types_.push_back(item_type);
    return instance_.item_types_.size() - 1;
}

void InstanceBuilder::set_item_type_allow_mirroring(
        ItemTypeId item_type_id,
        bool allow_mirroring)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].allow_mirroring = allow_mirroring;
}

void InstanceBuilder::add_item_type(
        const ItemType& item_type,
        Profit profit,
        ItemPos copies)
{
    ItemTypeId item_type_id = add_item_type(
            item_type.shapes,
            profit,
            copies,
            item_type.allowed_rotations);
    set_item_type_allow_mirroring(
            item_type_id,
            item_type.allow_mirroring);
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].area_orig;
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
                instance_.bin_type(bin_type_id).area_orig);
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
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + instance_path + "\".");
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
        if (json_parameters.contains("open_dimension_xy_aspect_ratio"))
            set_open_dimension_xy_aspect_ratio(json_parameters["open_dimension_xy_aspect_ratio"]);
    }

    // Read bin types.
    for (const auto& json_item: j["bin_types"]) {
        Shape shape = Shape::from_json(json_item);

        Profit cost = -1;
        if (json_item.contains("cost"))
            cost = json_item["cost"];

        BinPos copies = 1;
        if (json_item.contains("copies"))
            copies = json_item["copies"];

        BinPos copies_min = 0;
        if (json_item.contains("copies_min"))
            copies_min = json_item["copies_min"];

        LengthDbl item_bin_minimum_spacing = 0;
        if (json_item.contains("item_bin_minimum_spacing"))
            item_bin_minimum_spacing = json_item["item_bin_minimum_spacing"];

        BinTypeId bin_type_id = add_bin_type(shape, cost, copies, copies_min);
        set_item_bin_minimum_spacing(
                bin_type_id,
                item_bin_minimum_spacing);

        // Read defects.
        if (json_item.contains("defects")) {
            for (auto it_defect = json_item["defects"].begin();
                    it_defect != json_item["defects"].end();
                    ++it_defect) {
                auto json_defect = *it_defect;

                // Read type.
                DefectTypeId defect_type = -1;
                if (json_defect.contains("defect_type"))
                    defect_type = json_defect["defect_type"];

                LengthDbl item_defect_minimum_spacing = 0;
                if (json_defect.contains("item_defect_minimum_spacing"))
                    item_defect_minimum_spacing = json_item["item_defect_minimum_spacing"];

                // Read shape.
                ShapeWithHoles shape = ShapeWithHoles::from_json(json_defect);

                // Add defect.
                DefectId defect_id = add_defect(
                        bin_type_id,
                        defect_type,
                        shape);
                set_item_defect_minimum_spacing(
                        bin_type_id,
                        defect_id,
                        item_bin_minimum_spacing);
            }
        }
    }

    // Read item types.
    for (const auto& json_item: j["item_types"]) {
        std::vector<ItemShape> item_shapes;
        if (json_item.contains("shapes")) {
            // Multiple item shape.
            for (auto it_shape = json_item["shapes"].begin();
                    it_shape != json_item["shapes"].end();
                    ++it_shape) {
                ItemShape item_shape;
                item_shape.shape_orig = ShapeWithHoles::from_json(*it_shape);
                item_shapes.push_back(item_shape);
            }

        } else {
            // Single item shape.
            ItemShape item_shape;
            item_shape.shape_orig = ShapeWithHoles::from_json(json_item);
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
    // Compute scale value.
    if (instance_.parameters().scale_value == std::numeric_limits<LengthDbl>::infinity()) {
        LengthDbl value_max = 0.0;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance_.number_of_bin_types();
                ++bin_type_id) {
            BinType& bin_type = instance_.bin_types_[bin_type_id];
            auto mm = bin_type.shape_orig.compute_min_max();
            value_max = (std::max)(value_max, std::abs(mm.first.x));
            value_max = (std::max)(value_max, std::abs(mm.first.y));
            value_max = (std::max)(value_max, std::abs(mm.second.x));
            value_max = (std::max)(value_max, std::abs(mm.second.y));
        }
        instance_.parameters_.scale_value = 1024.0 / smallest_power_of_two_greater_or_equal(value_max);
        //std::cout << "instance_.scale_value() " << instance_.parameters().scale_value << std::endl;
    }

    // Compute scaled shapes of item type.
    AreaDbl smallest_item_area = std::numeric_limits<AreaDbl>::infinity();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];

        // Compute scaled shapes.
        for (ShapePos shape_pos = 0;
                shape_pos < (ShapePos)item_type.shapes.size();
                ++shape_pos) {
            ItemShape& item_shape = item_type.shapes[shape_pos];
            if (!item_shape.shape_scaled.shape.elements.empty())
                continue;

            item_shape.shape_scaled = instance_.parameters().scale_value * item_shape.shape_orig;
            smallest_item_area = (std::min)(smallest_item_area, item_shape.shape_scaled.shape.compute_area());
        }
    }

    // Remove small holes.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        item_type.area_scaled = 0;

        // Compute scaled shapes.
        for (ShapePos shape_pos = 0;
                shape_pos < (ShapePos)item_type.shapes.size();
                ++shape_pos) {
            ItemShape& item_shape = item_type.shapes[shape_pos];
            item_shape.shape_scaled = shape::remove_small_holes(
                    item_shape.shape_scaled,
                    smallest_item_area);
            item_type.area_scaled += item_shape.shape_scaled.compute_area();
        }
    }

    // Compute item type attributes.
    AreaDbl bin_types_area_max = compute_bin_types_area_max();
    instance_.all_item_types_infinite_copies_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];

        // Compute inflated shapes.
        for (ShapePos shape_pos = 0;
                shape_pos < (ShapePos)item_type.shapes.size();
                ++shape_pos) {
            ItemShape& item_shape = item_type.shapes[shape_pos];
            if (!item_shape.shape_inflated.shape.elements.empty())
                continue;
            item_shape.shape_inflated = inflate(
                    item_shape.shape_scaled,
                    instance_.parameters().scale_value * instance_.parameters().item_item_minimum_spacing);
            if (!item_shape.shape_inflated.shape.check()) {
                throw std::logic_error(
                        FUNC_SIGNATURE + ": invalid item inflated shape; "
                        "item_type_id: " + std::to_string(item_type_id) + "; "
                        "shape_pos: " + std::to_string(shape_pos) + ".");
            }

            if (item_shape.quality_rule < -1) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "invalid quality rule; "
                        "item_type_id: " + std::to_string(item_type_id) + "; "
                        "item_shape_pos: " + std::to_string(shape_pos) + "; "
                        "quality_rule: " + std::to_string(item_shape.quality_rule) + ".");
            }
            if (item_shape.quality_rule != -1
                    && item_shape.quality_rule >= instance_.parameters_.quality_rules.size()) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "invalid quality rule; "
                        "item_type_id: " + std::to_string(item_type_id) + "; "
                        "item_shape_pos: " + std::to_string(shape_pos) + "; "
                        "quality_rule: " + std::to_string(item_shape.quality_rule) + "; "
                        "parameters().quality_rules.size(): " + std::to_string(instance_.parameters().quality_rules.size()) + ".");
            }
        }

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
        // Update largest_item_profit_.
        instance_.largest_item_profit_ = std::max(instance_.largest_item_profit(), item_type.profit);
        // Update item_area_.
        instance_.item_area_ += item_type.copies * item_type.area_orig;
        // Update smallest_item_area_ and largest_item_area_.
        instance_.smallest_item_area_ = (std::min)(
                instance_.smallest_item_area_,
                item_type.area_orig);
        instance_.largest_item_area_ = (std::max)(
                instance_.largest_item_area_,
                item_type.area_orig);
        // Update largest_efficiency_item_type_.
        if (instance_.largest_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.largest_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.largest_efficiency_item_type_id_).area_orig
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).area_orig) {
            instance_.largest_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_area_max - 1) / item_type.area_orig + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update largest_item_copies_.
        if (instance_.largest_item_copies_ < item_type.copies)
            instance_.largest_item_copies_ = item_type.copies;
    }

    // Compute bin type attributes.
    instance_.bin_area_ = 0;
    AreaDbl previous_bins_area = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        BinType& bin_type = instance_.bin_types_[bin_type_id];

        // Compute scaled shapes.
        if (bin_type.shape_scaled.elements.empty()) {
            bin_type.shape_scaled = instance_.parameters().scale_value * bin_type.shape_orig;
            for (Defect& defect: bin_type.defects)
                defect.shape_scaled = instance_.parameters().scale_value * defect.shape_orig;
        }
        bin_type.area_scaled = bin_type.shape_scaled.compute_area();

        // Compute inflated defects.
        for (Defect& defect: bin_type.defects) {
            if (!defect.shape_inflated.shape.elements.empty())
                continue;
            defect.shape_inflated = inflate(
                    defect.shape_scaled,
                    instance_.parameters().scale_value * defect.item_defect_minimum_spacing);
        }

        // Compute inflated borders.
        if (bin_type.borders.empty()) {
            for (const Shape& shape_border: extract_borders(bin_type.shape_scaled)) {
                Defect border;
                border.type = -2;
                border.shape_scaled.shape = shape_border;
                border.shape_inflated = inflate(
                        shape_border,
                        instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing);
                bin_type.borders.push_back(border);
            }
        }

        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_area_sum_.
        instance_.bin_area_ += bin_type.copies * bin_type.area_orig;
        // Update previous_bins_area_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_area_.push_back(previous_bins_area);
            previous_bins_area += bin_type.area_orig;
        }
        // Update largest_bin_cost_.
        if (instance_.largest_bin_cost_ < bin_type.cost)
            instance_.largest_bin_cost_ = bin_type.cost;
        // Update number_of_defects_.
        instance_.number_of_defects_ += bin_type.defects.size();
    }

    if (instance_.objective() == Objective::OpenDimensionX
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the instance has objective OpenDimensionX and contains " + std::to_string(instance_.number_of_bins()) + " bins; "
                "an instance with objective OpenDimensionX must contain exactly one bin.");
    }
    if (instance_.objective() == Objective::OpenDimensionY
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the instance has objective OpenDimensionY and contains " + std::to_string(instance_.number_of_bins()) + " bins; "
                "an instance with objective OpenDimensionY must contain exactly one bin.");
    }

    return std::move(instance_);
}
