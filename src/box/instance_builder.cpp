#include "packingsolver/box/instance_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Parameters //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'x' must be > 0; "
                "x: " + std::to_string(x) + ".");
    }
    if (y <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'y' must be > 0; "
                "y: " + std::to_string(y) + ".");
    }
    if (z <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'z' must be > 0; "
                "z: " + std::to_string(z) + ".");
    }
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
    bin_type.box.x = x;
    bin_type.box.y = y;
    bin_type.box.z = z;
    bin_type.cost = (cost == -1)? x * y: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return instance_.bin_types_.size() - 1;
}

void InstanceBuilder::set_bin_type_maximum_weight(
        BinTypeId bin_type_id,
        Weight maximum_weight)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    instance_.bin_types_[bin_type_id].maximum_weight = maximum_weight;
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
        int rotations)
{
    if (x < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'x' must be > 0; "
                "x: " + std::to_string(x) + ".");
    }
    if (y < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'y' must be > 0; "
                "y: " + std::to_string(y) + ".");
    }
    if (z < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'z' must be > 0; "
                "z: " + std::to_string(z) + ".");
    }
    if (copies <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'copies' must be > 0; "
                "copies: " + std::to_string(copies) + ".");
    }

    ItemType item_type;
    item_type.box.x = x;
    item_type.box.y = y;
    item_type.box.z = z;
    item_type.profit = (profit == -1)? x * y * z: profit;
    item_type.copies = copies;
    item_type.rotations = rotations;
    instance_.item_types_.push_back(item_type);
    return instance_.item_types_.size() - 1;
}

void InstanceBuilder::set_item_type_weight(
        ItemTypeId item_type_id,
        Weight weight)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].weight = weight;
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
            item_type.rotations);
    set_item_type_weight(
            item_type_id,
            item_type.weight);
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
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + parameters_path + "\".");
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
        }
    }
}

void InstanceBuilder::read_bin_types(
        const std::string& bins_path)
{
    std::ifstream f(bins_path);
    if (!f.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + bins_path + "\".");
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
        Weight maximum_weight = std::numeric_limits<Weight>::infinity();

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "Z") {
                z = (Length)std::stol(line[i]);
            } else if (labels[i] == "COST") {
                cost = (Profit)std::stod(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_WEIGHT") {
                maximum_weight = (Weight)std::stod(line[i]);
            }
        }

        if (x == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" column in \"" + bins_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Y\" column in \"" + bins_path + "\".");
        }
        if (z == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Z\" column in \"" + bins_path + "\".");
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
    }
}

void InstanceBuilder::read_item_types(
        const std::string& items_path)
{
    std::ifstream f(items_path);
    if (!f.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + items_path + "\".");
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
            }
        }

        if (x == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" column in \"" + items_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Y\" column in \"" + items_path + "\".");
        }
        if (z == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Z\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = x * y * z;

        ItemTypeId item_type_id = add_item_type(
                x,
                y,
                z,
                profit,
                copies,
                rotations);
        set_item_type_weight(
                item_type_id,
                weight);
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
        // Update largest_item_profit_.
        instance_.largest_item_profit_ = std::max(instance_.largest_item_profit(), item_type.profit);
        // Update item_volume_.
        instance_.item_volume_ += item_type.copies * item_type.volume();
        // Update item_weight_.
        instance_.item_weight_ += item_type.copies * item_type.weight;
        // Update largest_efficiency_item_type_.
        if (instance_.largest_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.largest_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.largest_efficiency_item_type_id_).volume()
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).volume()) {
            instance_.largest_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_volume_max - 1) / item_type.volume() + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update largest_item_copies_.
        if (instance_.largest_item_copies_ < item_type.copies)
            instance_.largest_item_copies_ = item_type.copies;
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
        // Update largest_bin_cost_.
        if (instance_.largest_bin_cost_ < bin_type.cost)
            instance_.largest_bin_cost_ = bin_type.cost;
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
