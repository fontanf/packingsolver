#include "packingsolver/boxstacks/instance_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

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
        Length z,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (x <= 0) {
        throw std::runtime_error(
                "'boxstacks::InstanceBuilder::add_bin_type'"
                " requires 'x > 0'.");
    }
    if (y <= 0) {
        throw std::runtime_error(
                "'boxstacks::InstanceBuilder::add_bin_type'"
                " requires 'y > 0'.");
    }
    if (z <= 0) {
        throw std::runtime_error(
                "'boxstacks::InstanceBuilder::add_bin_type'"
                " requires 'z > 0'.");
    }
    if (cost < 0 && cost != -1) {
        throw std::runtime_error(
                "'boxstacks::InstanceBuilder::add_bin_type'"
                " requires 'cost >= 0' or 'cost == -1'.");
    }
    if (copies_min < 0) {
        throw std::runtime_error(
                "'boxstacks::InstanceBuilder::add_bin_type'"
                " requires 'copies_min >= 0'.");
    }
    if (copies != -1) {
        if (copies <= 0) {
            throw std::runtime_error(
                    "'boxstacks::InstanceBuilder::add_bin_type'"
                    " requires 'copies > 0' or 'copies == -1'.");
        }
        if (copies_min > copies) {
            throw std::runtime_error(
                    "'boxstacks::InstanceBuilder::add_bin_type'"
                    " requires 'copies_min <= copies' or 'copies == -1'.");
        }
    }

    BinType bin_type;
    bin_type.id = instance_.bin_types_.size();
    bin_type.box.x = x;
    bin_type.box.y = y;
    bin_type.box.z = z;
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

void InstanceBuilder::set_bin_type_maximum_stack_density(
        BinTypeId bin_type_id,
        double maximum_stack_density)
{
    instance_.bin_types_[bin_type_id].maximum_stack_density = maximum_stack_density;
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
                "boxstacks::InstanceBuilder::add_defect"
                ". bin_type_id: " + std::to_string(bin_type_id)
                + "; instance_.bin_types_.size(): "
                + std::to_string(instance_.bin_types_.size())
                + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];

    rectangle::Defect defect;
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
            bin_type.box.x,
            bin_type.box.y,
            bin_type.box.z,
            bin_type.cost,
            copies);
    set_bin_type_maximum_weight(
            bin_type_id,
            bin_type.maximum_weight);
    set_bin_type_maximum_stack_density(
            bin_type_id,
            bin_type.maximum_stack_density);
    set_bin_type_semi_trailer_truck_parameters(
            bin_type_id,
            bin_type.semi_trailer_truck_data);
    for (const auto& defect: bin_type.defects) {
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
        length += item_type.copies * item_type.box.max();
    }
    return length;
}

void InstanceBuilder::set_bin_types_infinite_x()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].box.x = length;
    }
}

void InstanceBuilder::set_bin_types_infinite_y()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].box.y = length;
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
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].volume();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Item types //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemTypeId InstanceBuilder::add_item_type(
        Length x,
        Length y,
        Length z,
        Profit profit,
        ItemPos copies,
        int rotations,
        GroupId group_id)
{
    if (x < 0) {
        throw std::invalid_argument(
                "boxstacks::InstanceBuilder::add_item_type."
                " item type x "
                + std::to_string(x)
                + " must be >= 0.");
    }
    if (y < 0) {
        throw std::invalid_argument(
                "boxstacks::InstanceBuilder::add_item_type."
                " item type y "
                + std::to_string(y)
                + " must be >= 0.");
    }
    if (z < 0) {
        throw std::invalid_argument(
                "boxstacks::InstanceBuilder::add_item_type."
                " item type z "
                + std::to_string(y)
                + " must be >= 0.");
    }
    if (copies <= 0) {
        throw std::invalid_argument(
                "boxstacks::InstanceBuilder::add_item_type."
                " item type copies "
                + std::to_string(copies)
                + " must be >= 0.");
    }
    if (group_id < 0) {
        throw std::invalid_argument(
                "boxstacks::InstanceBuilder::add_item_type."
                " item type group id "
                + std::to_string(group_id)
                + " must be >= 0.");
    }

    ItemType item_type;
    item_type.id = instance_.item_types_.size();
    item_type.box.x = x;
    item_type.box.y = y;
    item_type.box.z = z;
    item_type.profit = (profit == -1)? x * y * z: profit;
    item_type.copies = copies;
    item_type.group_id = group_id;
    item_type.rotations = rotations;
    instance_.item_types_.push_back(item_type);
    return item_type.id;
}

void InstanceBuilder::set_item_type_weight(
        ItemTypeId item_type_id,
        Weight weight)
{
    instance_.item_types_[item_type_id].weight = weight;
}

void InstanceBuilder::set_item_type_stackability_id(
        ItemTypeId item_type_id,
        StackabilityId stackability_id)
{
    instance_.item_types_[item_type_id].stackability_id = stackability_id;
}

void InstanceBuilder::set_item_type_nesting_height(
        ItemTypeId item_type_id,
        Length nesting_height)
{
    instance_.item_types_[item_type_id].nesting_height = nesting_height;
}

void InstanceBuilder::set_item_type_maximum_stackability(
        ItemTypeId item_type_id,
        ItemPos maximum_stackability)
{
    instance_.item_types_[item_type_id].maximum_stackability = maximum_stackability;
}

void InstanceBuilder::set_item_type_maximum_weight_above(
        ItemTypeId item_type_id,
        Weight maximum_weight_above)
{
    instance_.item_types_[item_type_id].maximum_weight_above = maximum_weight_above;
}

void InstanceBuilder::add_item_type(
        const ItemType& item_type,
        Profit profit,
        ItemPos copies)
{
    ItemTypeId item_type_id = add_item_type(
            item_type.box.x,
            item_type.box.y,
            item_type.box.z,
            profit,
            copies,
            item_type.rotations,
            item_type.group_id);
    set_item_type_weight(
            item_type_id,
            item_type.weight);
    set_item_type_stackability_id(
            item_type_id,
            item_type.stackability_id);
    set_item_type_nesting_height(
            item_type_id,
            item_type.nesting_height);
    set_item_type_maximum_stackability(
            item_type_id,
            item_type.maximum_stackability);
    set_item_type_maximum_weight_above(
            item_type_id,
            item_type.maximum_weight_above);
}

void InstanceBuilder::compute_bin_attributes()
{
    instance_.bin_volume_ = 0;
    instance_.bin_weight_ = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        instance_.bin_volume_ += bin_type.copies * bin_type.volume();
        instance_.bin_weight_ += bin_type.copies * bin_type.maximum_weight;
    }
}

void InstanceBuilder::set_item_types_profits_auto()
{
    compute_bin_attributes();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        Profit profit
            = (double)instance_.item_volume() / instance_.bin_volume()
            * (double)item_type.volume() / instance_.bin_volume()
            + (double)instance_.item_weight() / instance_.bin_weight()
            * (double)item_type.weight / instance_.bin_weight();
        instance_.item_types_[item_type_id].profit = profit;
    }
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].volume();
    }
}

void InstanceBuilder::set_item_types_oriented()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].rotations = 1;
    }
}

Volume InstanceBuilder::compute_bin_types_volume_max() const
{
    Volume bin_types_volume_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        bin_types_volume_max = std::max(
                bin_types_volume_max,
                instance_.bin_type(bin_type_id).volume());
    }
    return bin_types_volume_max;
}

void InstanceBuilder::set_item_types_infinite_copies()
{
    Volume bin_types_volume_max = compute_bin_types_volume_max();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        ItemPos c = (bin_types_volume_max - 1) / item_type.volume() + 1;
        item_type.copies = c;
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
        //std::cout << "name " << name << std::endl;
        if (name == "objective") {
            Objective objective;
            std::stringstream ss(value);
            ss >> objective;
            set_objective(objective);
        } if (name == "unloading-constraint") {
            rectangle::UnloadingConstraint unloading_constraint;
            std::stringstream ss(value);
            ss >> unloading_constraint;
            set_unloading_constraint(unloading_constraint);
        } if (name == "no-check-weight-constraints") {
            GroupId group_id = (GroupId)std::stol(value);
            set_group_weight_constraints(group_id, false);
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
        Length z = -1;
        Profit cost = -1;
        BinPos copies = 1;
        BinPos copies_min = 0;
        Weight maximum_weight = std::numeric_limits<Weight>::max();
        double maximum_stack_density = std::numeric_limits<double>::max();

        SemiTrailerTruckData semi_trailer_truck_data;
        semi_trailer_truck_data.is = true;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "Z") {
                z = (Length)std::stol(line[i]);
            } else if (labels[i] == "COST") {
                cost = (Profit)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_WEIGHT") {
                maximum_weight = (Weight)std::stod(line[i]);
            } else if (labels[i] == "MAXIMUM_STACK_DENSITY") {
                maximum_stack_density = (double)std::stod(line[i]);
            }
            semi_trailer_truck_data.read(labels[i], line[i]);
        }

        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"X\" column in \"" + bins_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    "Missing \"Y\" column in \"" + bins_path + "\".");
        }
        if (z == -1) {
            throw std::runtime_error(
                    "Missing \"Z\" column in \"" + bins_path + "\".");
        }

        BinTypeId bin_type_id = add_bin_type(
                x,
                y,
                z,
                cost,
                copies,
                copies_min);
        set_bin_type_maximum_weight(
                bin_type_id,
                maximum_weight);
        set_bin_type_maximum_stack_density(
                bin_type_id,
                maximum_stack_density);
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
                } else if (labels[c] == "LX") {
                    rect_x = (Length)std::stol(line[c]);
                } else if (labels[c] == "LY") {
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
                        "Missing \"LX\" column in \"" + defects_path + "\".");
            }
            if (rect_y == -1) {
                throw std::runtime_error(
                        "Missing \"LY\" column in \"" + defects_path + "\".");
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
        Length z = -1;
        Profit profit = -1;
        Weight weight = 0;
        ItemPos copies = 1;
        int rotations = 1;
        GroupId group_id = 0;
        StackabilityId stackability_id = 0;
        Length nesting_height = 0;
        ItemPos maximum_stackability = std::numeric_limits<ItemPos>::max();
        Weight maximum_weight_above = std::numeric_limits<Weight>::max();

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "Z") {
                z = (Length)std::stol(line[i]);
            } else if (labels[i] == "PROFIT") {
                profit = (Profit)std::stod(line[i]);
            } else if (labels[i] == "WEIGHT") {
                weight = (Weight)std::stod(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "ROTATIONS") {
                rotations = (int)std::stol(line[i]);
            } else if (labels[i] == "GROUP_ID") {
                group_id = (GroupId)std::stol(line[i]);
            } else if (labels[i] == "STACKABILITY_ID") {
                stackability_id = (StackabilityId)std::stol(line[i]);
            } else if (labels[i] == "NESTING_HEIGHT") {
                nesting_height = (Length)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_STACKABILITY") {
                maximum_stackability = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_WEIGHT_ABOVE") {
                maximum_weight_above = (Weight)std::stod(line[i]);
            }
        }

        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"X\" column in \"" + items_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    "Missing \"Y\" column in \"" + items_path + "\".");
        }
        if (z == -1) {
            throw std::runtime_error(
                    "Missing \"Z\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = x * y * z;

        ItemTypeId item_type_id = add_item_type(
                x,
                y,
                z,
                profit,
                copies,
                rotations,
                group_id);
        set_item_type_stackability_id(
                item_type_id,
                stackability_id);
        set_item_type_weight(
                item_type_id,
                weight);
        set_item_type_nesting_height(
                item_type_id,
                nesting_height);
        set_item_type_maximum_stackability(
                item_type_id,
                maximum_stackability);
        set_item_type_maximum_weight_above(
                item_type_id,
                maximum_weight_above);
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Instance InstanceBuilder::build()
{
    // Compute item type attributes.
    Volume bin_types_volume_max = compute_bin_types_volume_max();
    instance_.all_item_types_infinite_copies_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update item_volume_.
        instance_.item_volume_ += item_type.copies * item_type.volume();
        // Update item_weight_.
        instance_.item_weight_ += item_type.copies * item_type.weight;
        // Update max_efficiency_item_type_.
        if (instance_.max_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.max_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.max_efficiency_item_type_id_).volume()
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).volume()) {
            instance_.max_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_volume_max - 1) / item_type.volume() + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
    }

    // Compute bin type attributes.
    instance_.bin_volume_ = 0;
    instance_.bin_area_ = 0;
    instance_.bin_weight_ = 0;
    Volume previous_bins_volume = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_volume_.
        instance_.bin_volume_ += bin_type.copies * bin_type.volume();
        // Update bins_area_.
        instance_.bin_area_ += bin_type.copies * bin_type.area();
        // Update bin_weight_..
        instance_.bin_weight_ += bin_type.copies * bin_type.maximum_weight;
        // Update previous_bins_volume_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_volume_.push_back(previous_bins_volume);
            previous_bins_volume += bin_type.volume();
        }
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
                "boxstacks::InstanceBuilder::build."
                " The instance has objective OpenDimensionX and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionX must contain exactly one bin.");
    }
    if (instance_.objective() == Objective::OpenDimensionY
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "boxstacks::InstanceBuilder::build."
                " The instance has objective OpenDimensionY and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionY must contain exactly one bin.");
    }

    return std::move(instance_);
}
