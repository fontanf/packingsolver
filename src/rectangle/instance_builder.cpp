#include "packingsolver/rectangle/instance_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Parameters //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void InstanceBuilder::set_group_weight_constraints(
        GroupId group_id,
        bool check_weight_constraints)
{
    while ((GroupId)instance_.parameters_.check_weight_constraints.size() <= group_id)
        instance_.parameters_.check_weight_constraints.push_back(true);
    instance_.parameters_.check_weight_constraints[group_id] = check_weight_constraints;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Bin types ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinTypeId InstanceBuilder::add_bin_type(
        Length x,
        Length y,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (x <= 0) {
        throw std::runtime_error(
                "'rectangle::InstanceBuilder::add_bin_type'"
                " requires 'x > 0'.");
    }
    if (y <= 0) {
        throw std::runtime_error(
                "'rectangle::InstanceBuilder::add_bin_type'"
                " requires 'y > 0'.");
    }
    if (cost < 0 && cost != -1) {
        throw std::runtime_error(
                "'rectangle::InstanceBuilder::add_bin_type'"
                " requires 'cost >= 0' or 'cost == -1'.");
    }
    if (copies_min < 0) {
        throw std::runtime_error(
                "'rectangle::InstanceBuilder::add_bin_type'"
                " requires 'copies_min >= 0'.");
    }
    if (copies != -1) {
        if (copies <= 0) {
            throw std::runtime_error(
                    "'rectangle::InstanceBuilder::add_bin_type'"
                    " requires 'copies > 0' or 'copies == -1'.");
        }
        if (copies_min > copies) {
            throw std::runtime_error(
                    "'rectangle::InstanceBuilder::add_bin_type'"
                    " requires 'copies_min <= copies' or 'copies == -1'.");
        }
    }

    BinType bin_type;
    bin_type.id = instance_.bin_types_.size();
    bin_type.rect.x = x;
    bin_type.rect.y = y;
    bin_type.cost = (cost == -1)? x * y: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return bin_type.id;
}

void InstanceBuilder::set_bin_type_maximum_weight(
        BinTypeId bin_type_id,
        Weight maximum_weight)
{
    instance_.bin_types_[bin_type_id].maximum_weight = maximum_weight;
}

void InstanceBuilder::set_bin_type_semi_trailer_truck_parameters(
        BinTypeId bin_type_id,
        const SemiTrailerTruckData& semi_trailer_truck_data)
{
    instance_.bin_types_[bin_type_id].semi_trailer_truck_data = semi_trailer_truck_data;
}

void InstanceBuilder::add_defect(
        BinTypeId bin_type_id,
        Length pos_x,
        Length pos_y,
        Length rect_x,
        Length rect_y)
{
    if (bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::add_defect"
                ". bin_type_id: " + std::to_string(bin_type_id)
                + "; instance_.bin_types_.size(): "
                + std::to_string(instance_.bin_types_.size())
                + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];

    Defect defect;
    defect.id = bin_type.defects.size();
    defect.bin_type_id = bin_type_id;
    defect.pos.x = pos_x;
    defect.pos.y = pos_y;
    defect.rect.x = rect_x;
    defect.rect.y = rect_y;
    bin_type.defects.push_back(defect);
}

void InstanceBuilder::add_bin_type(
        const BinType& bin_type,
        BinPos copies)
{
    BinTypeId bin_type_id = add_bin_type(
            bin_type.rect.x,
            bin_type.rect.y,
            bin_type.cost,
            copies);
    set_bin_type_maximum_weight(
            bin_type_id,
            bin_type.maximum_weight);
    set_bin_type_semi_trailer_truck_parameters(
            bin_type_id,
            bin_type.semi_trailer_truck_data);
    for (const Defect& defect: bin_type.defects) {
        add_defect(
                bin_type_id,
                defect.pos.x,
                defect.pos.y,
                defect.rect.x,
                defect.rect.y);
    }
}

Length InstanceBuilder::compute_item_types_max_length_sum() const
{
    Length length = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        length += item_type.copies * item_type.rect.max();
    }
    return length;
}

void InstanceBuilder::set_bin_types_infinite_x()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].rect.x = length;
    }
}

void InstanceBuilder::set_bin_types_infinite_y()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].rect.y = length;
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
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].rect.area();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Item types //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemTypeId InstanceBuilder::add_item_type(
        Length x,
        Length y,
        Profit profit,
        ItemPos copies,
        bool oriented,
        GroupId group_id)
{
    if (x < 0) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::add_item_type."
                " item type width "
                + std::to_string(x)
                + " must be >= 0.");
    }
    if (y < 0) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::add_item_type."
                " item type height "
                + std::to_string(y)
                + " must be >= 0.");
    }
    if (copies <= 0) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::add_item_type."
                " item type copies "
                + std::to_string(copies)
                + " must be >= 0.");
    }
    if (group_id < 0) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::add_item_type."
                " item type group id "
                + std::to_string(group_id)
                + " must be >= 0.");
    }

    ItemType item_type;
    item_type.id = instance_.item_types_.size();
    item_type.rect.x = x;
    item_type.rect.y = y;
    item_type.profit = (profit == -1)? x * y: profit;
    item_type.copies = copies;
    item_type.group_id = group_id;
    item_type.oriented = oriented;
    instance_.item_types_.push_back(item_type);
    return item_type.id;
}

void InstanceBuilder::set_item_type_weight(
        ItemTypeId item_type_id,
        Weight weight)
{
    instance_.item_types_[item_type_id].weight = weight;
}

void InstanceBuilder::add_item_type(
        const ItemType& item_type,
        Profit profit,
        ItemPos copies)
{
    ItemTypeId item_type_id = add_item_type(
            item_type.rect.x,
            item_type.rect.y,
            profit,
            copies,
            item_type.oriented);
    set_item_type_weight(
            item_type_id,
            item_type.weight);
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].rect.area();
    }
}

void InstanceBuilder::set_item_types_oriented()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].oriented = true;
    }
}

Area InstanceBuilder::compute_bin_types_area_max() const
{
    Area bin_types_area_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        bin_types_area_max = std::max(
                bin_types_area_max,
                instance_.bin_type(bin_type_id).area());
    }
    return bin_types_area_max;
}

void InstanceBuilder::set_item_types_infinite_copies()
{
    Area bin_types_area_max = compute_bin_types_area_max();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        ItemPos c = (bin_types_area_max - 1) / item_type.area() + 1;
        item_type.copies = c;
    }
}

void InstanceBuilder::multiply_item_types_copies(ItemPos factor)
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        item_type.copies *= factor;
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Read from files ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void InstanceBuilder::read_parameters(
        const std::string& parameters_path)
{
    if (parameters_path.empty())
        return;

    std::ifstream f(parameters_path);
    if (parameters_path != "" && !f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + parameters_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        std::string name;
        std::string value;

        for (Counter c = 0; c < (Counter)line.size(); ++c) {
            if (labels[c] == "NAME") {
                name = line[c];
            } else if (labels[c] == "VALUE") {
                value = line[c];
            }
        }

        if (name == "objective") {
            Objective objective;
            std::stringstream ss(value);
            ss >> objective;
            set_objective(objective);
        } else if (name == "unloading_constraint") {
            rectangle::UnloadingConstraint unloading_constraint;
            std::stringstream ss(value);
            ss >> unloading_constraint;
            set_unloading_constraint(unloading_constraint);
        }
    }
}

void InstanceBuilder::read_bin_types(
        const std::string& bins_path)
{
    std::ifstream f(bins_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + bins_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read bin file
    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        Length x = -1;
        Length y = -1;
        Profit cost = -1;
        BinPos copies = 1;
        BinPos copies_min = 0;
        SemiTrailerTruckData semi_trailer_truck_data;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "COST") {
                cost = (Profit)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (BinPos)std::stol(line[i]);
            }
            semi_trailer_truck_data.read(labels[i], line[i]);
        }

        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"WIDTH\" column in \"" + bins_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    "Missing \"HEIGHT\" column in \"" + bins_path + "\".");
        }

        BinTypeId bin_type_id = add_bin_type(
                x,
                y,
                cost,
                copies,
                copies_min);
        set_bin_type_semi_trailer_truck_parameters(
                bin_type_id,
                semi_trailer_truck_data);
    }
}

void InstanceBuilder::read_defects(
        const std::string& defects_path)
{
    std::ifstream f(defects_path);
    if (defects_path != "" && !f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + defects_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read defects file
    if (defects_path != "") {
        getline(f, tmp);
        labels = optimizationtools::split(tmp, ',');
        while (getline(f, tmp)) {
            line = optimizationtools::split(tmp, ',');

            BinTypeId bin_type_id = -1;
            Length pos_x = -1;
            Length pos_y = -1;
            Length rect_x = -1;
            Length rect_y = -1;

            for (Counter c = 0; c < (Counter)line.size(); ++c) {
                if (labels[c] == "BIN") {
                    bin_type_id = (BinTypeId)std::stol(line[c]);
                } else if (labels[c] == "X") {
                    pos_x = (Length)std::stol(line[c]);
                } else if (labels[c] == "Y") {
                    pos_y = (Length)std::stol(line[c]);
                } else if (labels[c] == "WIDTH") {
                    rect_x = (Length)std::stol(line[c]);
                } else if (labels[c] == "HEIGHT") {
                    rect_y = (Length)std::stol(line[c]);
                }
            }

            if (bin_type_id == -1) {
                throw std::runtime_error(
                        "Missing \"BIN\" column in \"" + defects_path + "\".");
            }
            if (pos_x == -1) {
                throw std::runtime_error(
                        "Missing \"X\" column in \"" + defects_path + "\".");
            }
            if (pos_y == -1) {
                throw std::runtime_error(
                        "Missing \"Y\" column in \"" + defects_path + "\".");
            }
            if (rect_x == -1) {
                throw std::runtime_error(
                        "Missing \"WIDTH\" column in \"" + defects_path + "\".");
            }
            if (rect_y == -1) {
                throw std::runtime_error(
                        "Missing \"HEIGHT\" column in \"" + defects_path + "\".");
            }

            add_defect(bin_type_id, pos_x, pos_y, rect_x, rect_y);
        }
    }

}

void InstanceBuilder::read_item_types(
        const std::string& items_path)
{
    std::ifstream f(items_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + items_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        Length x = -1;
        Length y = -1;
        Profit profit = -1;
        ItemPos copies = 1;
        bool oriented = false;
        GroupId group_id = 0;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "PROFIT") {
                profit = (Profit)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "ORIENTED") {
                oriented = (bool)std::stol(line[i]);
            } else if (labels[i] == "GROUP_ID") {
                group_id = (GroupId)std::stol(line[i]);
            }
        }

        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"WIDTH\" column in \"" + items_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    "Missing \"HEIGHT\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = x * y;

        add_item_type(x, y, profit, copies, oriented, group_id);
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Instance InstanceBuilder::build()
{
    // Compute item type attributes.
    Area bin_types_area_max = compute_bin_types_area_max();
    instance_.all_item_types_infinite_copies_ = true;
    instance_.smallest_item_width_ = std::numeric_limits<Length>::max();
    instance_.smallest_item_height_ = std::numeric_limits<Length>::max();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update item_area_.
        instance_.item_area_ += item_type.copies * item_type.area();
        // Update total_item_width_ and total_item_height_.
        instance_.total_item_width_ += item_type.copies * item_type.rect.x;
        instance_.total_item_height_ += item_type.copies * item_type.rect.y;
        // Update smallest_item_width_ and smallest_item_height_.
        instance_.smallest_item_width_ = std::min(instance_.smallest_item_width_, item_type.rect.x);
        instance_.smallest_item_height_ = std::min(instance_.smallest_item_height_, item_type.rect.y);
        if (!item_type.oriented) {
            instance_.smallest_item_width_ = std::min(instance_.smallest_item_width_, item_type.rect.y);
            instance_.smallest_item_height_ = std::min(instance_.smallest_item_height_, item_type.rect.x);
        }
        // Update max_efficiency_item_type_.
        if (instance_.max_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.max_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.max_efficiency_item_type_id_).area()
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).area()) {
            instance_.max_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_area_max - 1) / item_type.area() + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update maximum_item_copies_.
        if (instance_.maximum_item_copies_ < item_type.copies)
            instance_.maximum_item_copies_ = item_type.copies;
    }

    // Compute bin type attributes.
    instance_.bin_area_ = 0;
    Volume previous_bins_area = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_area_sum_.
        instance_.bin_area_ += bin_type.copies * bin_type.area();
        // Update previous_bins_area_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_area_.push_back(previous_bins_area);
            previous_bins_area += bin_type.area();
        }
        // Update maximum_bin_cost_.
        if (instance_.maximum_bin_cost_ < bin_type.cost)
            instance_.maximum_bin_cost_ = bin_type.cost;
        // Update number_of_defects_.
        instance_.number_of_defects_ += bin_type.defects.size();
    }

    // Compute number_of_groups_.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        while ((GroupId)instance_.groups_.size() <= item_type.group_id) {
            Group group;
            group.id = instance_.groups_.size();
            instance_.groups_.push_back(group);
        }
        Group& group = instance_.groups_[item_type.group_id];
        group.item_types.push_back(item_type_id);
        group.number_of_items += item_type.copies;
    }

    if (instance_.objective() == Objective::OpenDimensionX
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::build."
                " The instance has objective OpenDimensionX and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionX must contain exactly one bin.");
    }
    if (instance_.objective() == Objective::OpenDimensionY
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "rectangle::InstanceBuilder::build."
                " The instance has objective OpenDimensionY and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionY must contain exactly one bin.");
    }

    return std::move(instance_);
}
